#include "ArenaRoom.h"
#include "./ArenaRoom_Entity.h"
#include "./ArenaRoom_PacketDef.h"

using namespace arena_room;

ArenaRoom::WallEntity walls[4] = {};

ArenaRoom::ArenaRoom(ConnSession* pMasterSession, int capacity)
	: Room(pMasterSession, eRoomType::Arena, capacity)
	, m_entityRegistery{}
	, m_playerList{}
	, m_wallList{}
	, m_attackList{}
	, m_spawnPosIndex(0)
	, m_spawnPoses{}
{
	m_entityRegistery.Init();

	// 벽 콜라이더 생성
	walls[0].Init(Vec2f(-16.5f, -9.8f), Vec2f(0.6f, 20.4f));		// left
	walls[1].Init(Vec2f(16.5f, -9.8f), Vec2f(0.6f, 20.4f));			// right
	walls[2].Init(Vec2f(0.0f, 10.0f), Vec2f(32.4f, 0.6f));			// up
	walls[3].Init(Vec2f(0.0f, -9.8f), Vec2f(32.4f, 0.6f));			// down
	
	AppendToList(&m_wallList, &walls[0].link);
	AppendToList(&m_wallList, &walls[1].link);
	AppendToList(&m_wallList, &walls[2].link);
	AppendToList(&m_wallList, &walls[3].link);

	// 플레이어 부활 위치 설정
	m_spawnPoses[0] = Vec2f(0.0f, 0.0f);
	m_spawnPoses[1] = Vec2f(-14.0f, 8.0f);
	m_spawnPoses[2] = Vec2f(14.0f, 8.0f);
	m_spawnPoses[3] = Vec2f(-14.0f, -8.0f);
	m_spawnPoses[4] = Vec2f(14.0f, -8.0f);
}

ArenaRoom::~ArenaRoom()
{
	// 원거리 공격 entity를 삭제한다. 
	Link* pLink = m_attackList.pHead;
	while (pLink != nullptr)
	{
		Link* pNextLink = pLink->pNext;
		AttackEntity* pAttack = (AttackEntity*)pLink->pItem;

		UnlinkFromList(&m_attackList, &pAttack->link);
		delete pAttack;

		pLink = pNextLink;
	}

	// 모든 유저를 퇴장시킨다. 
	leaveAll(eLeaveReason::RoomDestroyed, true);

	// entityID 맵을 정리한다. 
	m_entityRegistery.CleanUp();
}

bool ArenaRoom::Update(float dt)
{
	// 플레이어 업데이트 처리: 현재 속도를 고려해 충돌처리르 한다. 
	Link* pLink = m_sessionList.pHead;
	while (pLink != nullptr)
	{
		Link* pNextLink = pLink->pNext;
		ConnSession* pSession = (ConnSession*)pLink->pItem;
		PlayerEntity* pPlayer = (PlayerEntity*)pSession->pRoomSession;
		if (pPlayer->IsAlive())
			updatePlayerEntity(dt, pPlayer);

		pLink = pNextLink;
	}

	// 원거리 공격 업데이트: 진행 방향으로 이동시키며 충돌 처리 및 피격 판정 처리한다. 
	pLink = m_attackList.pHead;
	while (pLink != nullptr)
	{
		Link* pNextLink = pLink->pNext;
		AttackEntity* pAttackEntity = (AttackEntity*)pLink->pItem;
		updateAttackEntity(dt, pAttackEntity);

		pLink = pNextLink;
	}

	return true;
}

bool ArenaRoom::HandlePacket(NetConn* pSender, Packet& packet, bool bLog)
{
	// opcode 값에 따라 적절한 handler method를 호출한다. 
	Opcode opcode = packet.GetOpcode();
	if (IsIt<CS_StartMove>(opcode))
		return handleStartMove(pSender, (CS_StartMove&)packet, false);
	else if (IsIt<CS_StopMove>(opcode))
		return handleStopMove(pSender, (CS_StopMove&)packet, false);
	else if (IsIt<CS_Dash>(opcode))
		return handleDash(pSender, (CS_Dash&)packet, bLog);
	else if (IsIt<CS_MeleeAttack>(opcode))
		return handleMeleeAttack(pSender, (CS_MeleeAttack&)packet, false);
	else if (IsIt<CS_ProjectileAttack>(opcode))
		return handleProjectileAttack(pSender, (CS_ProjectileAttack&)packet, bLog);
	else if (IsIt<CS_RoomChat>(opcode))
		return handleRoomChat(pSender, (CS_RoomChat&)packet, bLog);
	else if (IsIt<CS_SpawnLocalPlayer>(opcode))
		return handleSpawnLocalPlayer(pSender, (CS_SpawnLocalPlayer&)packet, bLog);
	else if (IsIt<CS_HpPortion>(opcode))
		return handleHpPortion(pSender, (CS_HpPortion&)packet, bLog);

	return true;
}

bool ArenaRoom::handleStartMove(NetConn* pSender, CS_StartMove& packet, bool bLog)
{
	assert(pSender != nullptr);

	// CS_StartMove 패킷의 velocity는 0이 아니여야 한다. 
	if (packet.vel == Vec2f(0.0f, 0.0f))
	{
		pSender->Disconnect();

		return true;
	}

	PlayerEntity* pPlayer = getPlayerEntity(pSender);
	if (pPlayer->IsAlive() == false)	// 이미 사망한 플레이어의 경우 무시한다. 
		return true;

	// 디버깅용 로그
	if (bLog || pPlayer->GetNickname() == L"Hello")
	{
		printf("[info]CS_StartMove connID: %d \n", pSender->GetID());
		Vec2f error = packet.pos - pPlayer->Pos;
		wprintf(L" - error X : %f \n", error.x);
		wprintf(L" - error Y : %f \n", error.y);
	}

	// 대쉬중이었다면 취소한다. 
	pPlayer->bIsDashing = false;

	// 클라이언트가 보낸 위치가 벽에 끼어있는지 확인한다. 
	Vec2f posBackup = pPlayer->Pos;
	pPlayer->Dir = packet.vel;
	pPlayer->Pos = packet.pos;
	void* pRes = isInWall(pPlayer);
	if (pRes != nullptr)    // 디버깅용 assert 처리
		DebugBreak();
	pPlayer->Vel = packet.vel;

	// 다른 유저에게 해당 유저의 새로운 위치와 속도를 보낸다. 
	SC_StartMove sc_startMove;
	sc_startMove.objectID = pPlayer->entityID;
	sc_startMove.pos = pPlayer->Pos;
	sc_startMove.vel = pPlayer->Dir;
	Link* pLink = m_sessionList.pHead;
	while (pLink != nullptr)
	{
		ConnSession* pSession_i = (ConnSession*)pLink->pItem;
		pLink = pLink->pNext;

		PlayerEntity* pPlayer_i = getPlayerEntity(pSession_i);
		if (pPlayer_i == pPlayer)
			continue;

		pSession_i->Send(sc_startMove, true);
	}

	return true;
}

bool ArenaRoom::handleStopMove(NetConn* pSender, CS_StopMove& packet, bool bLog)
{
	assert(pSender != nullptr);

	PlayerEntity* pPlayer = getPlayerEntity(pSender);
	if (pPlayer->IsAlive() == false)    // 이미 사망한 플레이어의 경우 무시한다. 
		return true;

	// 디버깅용 로그
	if (bLog || pPlayer->GetNickname() == L"Hello")
	{
		printf("[info]CS_StopMove connID: %d \n", pSender->GetID());
		Vec2f error = packet.pos - pPlayer->Pos;
		wprintf(L" - error X : %f \n", error.x);
		wprintf(L" - error Y : %f \n", error.y);
		wprintf(L" - isDashing : %d \n", pPlayer->bIsDashing);
	}

	// 대쉬중이었다면 취소한다. 
	pPlayer->bIsDashing = false;

	// 클라이언트가 보낸 위치가 벽에 끼어있는지 확인한다. 
	Vec2f posBackup = pPlayer->Pos;
	pPlayer->Vel = Vec2f();
	pPlayer->Pos = packet.pos;
	void* pRes = isInWall(pPlayer);
	if (pRes != nullptr)    // 디버깅용 assert 처리
		DebugBreak();
	pPlayer->Dir = packet.dir;

	// 다른 유저에게 해당 유저의 새로운 위치와 속도를 보낸다. 
	SC_StopMove sc_stopMove;
	sc_stopMove.objectID = pPlayer->entityID;
	sc_stopMove.pos = pPlayer->Pos;
	sc_stopMove.dir = pPlayer->Dir;
	Link* pLink = m_sessionList.pHead;
	while (pLink != nullptr)
	{
		ConnSession* pSession_i = (ConnSession*)pLink->pItem;
		pLink = pLink->pNext;

		PlayerEntity* pPlayer_i = getPlayerEntity(pSession_i);
		if (pPlayer_i == pPlayer)
			continue;

		pSession_i->Send(sc_stopMove, true);
	}

	return true;
}

bool ArenaRoom::handleDash(NetConn* pSender, CS_Dash& packet, bool bLog)
{
	PlayerEntity* pPlayer = getPlayerEntity(pSender);
	if (pPlayer->IsAlive() == false)    // 이미 사망한 플레이어의 경우 무시한다. 
		return true;

	// 플레이어는 dash가 끝나면 항상 스스로 패킷을 보내기 때문에, 중복으로 보내면 쫒아내도 된다. 
	if (pPlayer->bIsDashing)
	{
		pSender->Disconnect();

		return true;
	}

	if (bLog)
	{
		printf("[info]CS_Dash connID: %d \n", pSender->GetID());
	}

	// 클라이언트가 보낸 위치가 벽에 끼어있는지 확인한다. 
	Vec2f posBackup = pPlayer->Pos;
	pPlayer->Pos = packet.pos;			// 대쉬 시작 위치 설정
	void* pRes = isInWall(pPlayer);
	if (pRes != nullptr)
		DebugBreak();

	// 지형과 충돌처리를 통해 대쉬 끝 위치를 직접 계산한다. 
	Vec2f dashDelta = packet.dir.Normalized() * packet.DashDistance;
	CollisionInfo info;
	pPlayer->Sweep(m_wallList, dashDelta, &info);
	pPlayer->Apply(info, &dashDelta);

	pPlayer->bIsDashing = true;
	pPlayer->DashPos = pPlayer->Pos;			// 대쉬 끝 위치
	pPlayer->Pos = packet.pos;

	pPlayer->DashSpeed = packet.DashSpeed;

	// 다른 유저에게 해당 유저의 대쉬 패킷을 보낸다. 
	SC_Dash dash;
	dash.entityID = pPlayer->entityID;
	dash.speed = packet.DashSpeed;
	dash.startPos = pPlayer->Pos;
	dash.endPos = pPlayer->DashPos;
	Link* pLink = m_sessionList.pHead;
	while (pLink != nullptr)
	{
		Link* pNextLink = pLink->pNext;
		ConnSession* pSession_i = (ConnSession*)pLink->pItem;
		pSession_i->Send(dash, true);

		pLink = pNextLink;
	}

	return true;
}

bool ArenaRoom::handleMeleeAttack(NetConn* pSender, CS_MeleeAttack& packet, bool bLog)
{
	if (packet.Damage < 0)    // invalid packet
	{
		pSender->Disconnect();

		return true;
	}

	if (bLog)
	{
		printf("[info]CS_MeleeAttack connID: %d \n", pSender->GetID());
	}

	PlayerEntity* pPlayer = getPlayerEntity(pSender);
	if (pPlayer->IsAlive() == false)    // 이미 사망한 플레이어의 경우 무시한다. 
		return true;

	// 대쉬중이었다면 취소한다. 
	pPlayer->bIsDashing = false;

	// 클라이언트가 보낸 위치가 벽에 끼어있는지 확인한다. 
	packet.Dir.Normalize();
	Vec2f posBackup = pPlayer->Pos;
	pPlayer->Dir = packet.Dir;
	pPlayer->Pos = packet.Pos;
	pPlayer->Vel = Vec2f(0.0f, 0.0f);
	void* pRes = isInWall(pPlayer);
	if (pRes != nullptr)
		DebugBreak();

	// 근접 공격 AABB를 계산한다. 
	Vec2f pos = packet.Pos + Vec2f(0.0f, pPlayer->Size.y / 2.0f) + packet.Dir * 1.4f;
	Vec2f size(2.0f, 2.0f);
	Aabb2D aabb(pos, size);
	aabb.Pos -= aabb.Size / 2.0f;	// center에 있는 pos를 Aabb2D의 기준인 bottomLeft로 이동시킨다. 

	// 다른 유저에게 해당 유저의 근접 공격 패킷을 보낸다. 
	SC_MeleeAttack sc_packet;
	sc_packet.EntityID = pPlayer->entityID;
	sc_packet.Pos = pPlayer->Pos;
	sc_packet.Dir = packet.Dir;
	sc_packet.Speed = packet.Speed;
	Link* pLink = m_playerList.pHead;
	while (pLink != nullptr)
	{
		Link* pNextLink = pLink->pNext;

		// 본인이거나 이미 죽었거나 대쉬 중인 유저와는 근접 공격 히트 체크를 하지 않는다. 
		PlayerEntity* pPlayer_i = (PlayerEntity*)pLink->pItem;
		if (
			pPlayer_i == pPlayer || 
			pPlayer_i->IsAlive() == false ||
			pPlayer_i->bIsDashing == true
		)
		{
			pPlayer_i->Send(sc_packet, true);    // 패킷만 보낸다. 
			pLink = pNextLink;

			continue;
		}

		// 근접 공격 AABB와 overlap 확인 후 근접 공격 처리
		Aabb2D playerAabb_i = pPlayer_i->GetAabb2D(0.0f);
		if (aabb.IsOverlap(playerAabb_i, true))
		{
			player_vs_melee_attack(pPlayer, pPlayer_i, packet.Damage);
			if (pPlayer->IsAlive() == false)
				break;
		}

		pPlayer_i->Send(sc_packet, true);

		pLink = pNextLink;
	}


	return true;
}

bool ArenaRoom::handleProjectileAttack(NetConn* pSender, CS_ProjectileAttack& packet, bool bLog)
{
	if (packet.Damage < 0)    // invalid packet
	{
		pSender->Disconnect();

		return true;
	}

	PlayerEntity* pPlayer = getPlayerEntity(pSender);
	if (pPlayer->IsAlive() == false)    // 이미 사망한 플레이어의 경우 무시한다. 
		return true;

	// 대쉬중이었다면 취소한다. 
	pPlayer->bIsDashing = false;

	// 클라이언트가 보낸 위치가 벽에 끼어있는지 확인한다. 
	packet.dir.Normalize();
	Vec2f posBackup = pPlayer->Pos;
	pPlayer->Dir = packet.dir;
	pPlayer->Pos = packet.pos;
	void* pRes = isInWall(pPlayer);
	if (pRes != nullptr)
		DebugBreak();

	// 원거리 공격 entity의 생성 위치를 계산한다. 
	Vec2f pos = packet.pos + Vec2f(0.0f, pPlayer->Size.y / 2.0f) + packet.dir * 0.5f;
	Vec2f vel = packet.dir * packet.speed;

	// 원거리 공격 entity 초기화
	AttackEntity* pAttack = new AttackEntity(    // TODO: create from pool
		pPlayer->entityID, 
		pos,					// pos
		Vec2f(0.5f, 0.5f),		// size
		vel
	);
	pAttack->Damage = packet.Damage;
	EntityID projectileID = m_entityRegistery.Register(pAttack);
	assert(projectileID != INVALID_ENTITY_ID);
	pAttack->PlayerRegisterCount = pPlayer->RegisterCount;
	AppendToList(&m_attackList, &pAttack->link);

	// 다른 유저에게 해당 유저의 원거리 공격 패킷을 보낸다. 
	SC_ProjectileAttack resPacket;
	resPacket.entityID = projectileID;
	resPacket.playerID = pPlayer->entityID;
	resPacket.pos = pos;
	resPacket.vel = vel;
	Link* pLink = m_sessionList.pHead;
	while (pLink != nullptr)
	{
		Link* pNextLink = pLink->pNext;
		ConnSession* pSession_i = (ConnSession*)pLink->pItem;
		pSession_i->Send(resPacket, true);

		pLink = pNextLink;
	}

	return true;
}

bool ArenaRoom::handleRoomChat(NetConn* pSender, CS_RoomChat& packet, bool bLog)
{
	SC_RoomChat sc_packet;
	ConnSession* pSession = GetConnSession(pSender);
	sc_packet.nickname = pSession->GetNickname();
	sc_packet.msg = packet.msg;

	// 다른 유저에게 해당 유저의 채팅 패킷을 보낸다. 
	Link* pLink = m_sessionList.pHead;
	while (pLink != nullptr)
	{
		ConnSession* pSesion_i = (ConnSession*)pLink->pItem;
		pLink = pLink->pNext;
		if (pSesion_i == pSession)
			continue;
		
		pSesion_i->Send(sc_packet, false);
	}

	return true;
}

bool ArenaRoom::handleSpawnLocalPlayer(NetConn* pSender, CS_SpawnLocalPlayer& packet, bool bLog)
{
	PlayerEntity* pPlayer = getPlayerEntity(pSender);
	if (pPlayer->IsAlive())
	{
		pSender->Disconnect();

		return true;
	}

	// 스폰 위치 설정
	Vec2f spawnPos = getSpawnPos();
	pPlayer->Pos = spawnPos;
	pPlayer->Dir = Vec2f(0.0f, -1.0f);
	pPlayer->Vel = Vec2f(0.0f, 0.0f);
	pPlayer->MaxHp = PLAYER_HP;
	pPlayer->Hp = PLAYER_HP;
	pPlayer->bIsDashing = false;

	AppendToList(&m_playerList, &pPlayer->link);

	// 부활한 플레이어 본인에게 캐릭터 스폰을 알린다. 
	SC_SpawnLocalPlayer sc_spawnLocalPlayer;
	sc_spawnLocalPlayer.entityID = pPlayer->entityID;
	sc_spawnLocalPlayer.pos = pPlayer->Pos;
	sc_spawnLocalPlayer.dir = pPlayer->Dir;
	sc_spawnLocalPlayer.MaxHp = pPlayer->MaxHp;
	sc_spawnLocalPlayer.Hp = pPlayer->Hp;
	pSender->Send(sc_spawnLocalPlayer, true);

	// 다른 유저들에게 새 유저 스폰을 알린다. 
	SC_SpawnRemotePlayer sc_spawnRemotePlayer;
	sc_spawnRemotePlayer.entityID = pPlayer->entityID;
	sc_spawnRemotePlayer.nickname = pPlayer->GetNickname();
	sc_spawnRemotePlayer.characterClass = pPlayer->GetCharacterClass();
	sc_spawnRemotePlayer.pos = pPlayer->Pos;
	sc_spawnRemotePlayer.dir = pPlayer->Dir;
	sc_spawnRemotePlayer.MaxHp = pPlayer->MaxHp;
	sc_spawnRemotePlayer.Hp = pPlayer->Hp;
	Link* pLink = m_sessionList.pHead;
	while (pLink != nullptr)
	{
		Link* pNextLink = pLink->pNext;
		ConnSession* pSession_i = (ConnSession*)pLink->pItem;
		if (pSession_i == pPlayer->pConnSession)
		{
			pLink = pNextLink;

			continue;
		}

		PlayerEntity* pPlayer_i = (PlayerEntity*)pSession_i->pRoomSession;
		pPlayer_i->Send(sc_spawnRemotePlayer, true);

		pLink = pNextLink;
	}
	
	return true;
}

bool ArenaRoom::handleHpPortion(NetConn* pSender, arena_room::CS_HpPortion& packet, bool bLog)
{
	PlayerEntity* pPlayer = getPlayerEntity(pSender);
	if (pPlayer->IsAlive() == false)
		return true;

	// hp 회복 처리
	int oldHp = pPlayer->Hp;
	pPlayer->Hp += packet.Amount;
	pPlayer->Hp = std::min(pPlayer->Hp, pPlayer->MaxHp);
	SC_HpPortion sc_packet;
	sc_packet.EntityID = pPlayer->entityID;
	sc_packet.Amount = pPlayer->Hp - oldHp;
	sc_packet.Hp = pPlayer->Hp;
	Link* pLink = m_sessionList.pHead;
	while (pLink != nullptr)
	{
		// 클라이언트는 서버에서 HP 회복 확인 패킷을 받은 후 부터 쿨타임을 시작한다. 
		Link* pNextLink = pLink->pNext;
		ConnSession* pSession_i = (ConnSession*)pLink->pItem;
		pSession_i->Send(sc_packet, true);

		pLink = pNextLink;
	}

	return true;
}

bool ArenaRoom::enterRoom(ConnSession* pSession, eBroadcastTarget broadcastTarget)
{
	bool bRet = Room::enterRoom(pSession, broadcastTarget);
	if (bRet == false)
		return false;

	Vec2f spawnPos = getSpawnPos();
	PlayerEntity* pNewPlayer = new PlayerEntity(pSession, PLAYER_HP, spawnPos);
	pSession->pRoomSession = pNewPlayer;
	EntityID playerID = m_entityRegistery.Register(pNewPlayer);
	assert(playerID != INVALID_ENTITY_ID);
	pNewPlayer->RegisterCount = m_entityRegistery.registerCount;

	// 입장한 플레이어 본인에게 캐릭터 스폰을 알린다. 
	SC_SpawnLocalPlayer sc_spawnLocalPlayer;
	sc_spawnLocalPlayer.entityID = playerID;
	sc_spawnLocalPlayer.pos = pNewPlayer->Pos;
	sc_spawnLocalPlayer.dir = pNewPlayer->Dir;
	sc_spawnLocalPlayer.MaxHp = pNewPlayer->MaxHp;
	sc_spawnLocalPlayer.Hp = pNewPlayer->Hp;
	pNewPlayer->Send(sc_spawnLocalPlayer, true);

	// 다른 유저들에게 새 유저 스폰을 알린다. 
	SC_SpawnRemotePlayer sc_spawnRemotePlayer;
	sc_spawnRemotePlayer.entityID = playerID;
	sc_spawnRemotePlayer.nickname = pNewPlayer->GetNickname();
	sc_spawnRemotePlayer.characterClass = pNewPlayer->GetCharacterClass();
	sc_spawnRemotePlayer.pos = pNewPlayer->Pos;
	sc_spawnRemotePlayer.dir = pNewPlayer->Dir;
	sc_spawnRemotePlayer.MaxHp = pNewPlayer->MaxHp;
	sc_spawnRemotePlayer.Hp = pNewPlayer->Hp;
	Link* pLink = m_sessionList.pHead;
	while (pLink != nullptr)
	{
		Link* pNextLink = pLink->pNext;
		ConnSession* pSession_i = (ConnSession*)pLink->pItem;
		if (pSession_i == pSession)
		{
			pLink = pNextLink;

			continue;
		}

		PlayerEntity* pPlayer_i = (PlayerEntity*)pSession_i->pRoomSession;
		pPlayer_i->Send(sc_spawnRemotePlayer, true);

		if (pPlayer_i->IsAlive())
		{
			// 새로 들어온 유저에게 기존 유저들을 알려준다. 
			SC_PlayerEnterSight sc_playerEnterSight;
			sc_playerEnterSight.entityID = pPlayer_i->entityID;
			PlayerCharacter* pCharacter_i = pSession_i->pPlayerCharacter;
			sc_playerEnterSight.nickname = pCharacter_i->GetNickname();
			sc_playerEnterSight.characterClass = pCharacter_i->characterClass;
			sc_playerEnterSight.pos = pPlayer_i->Pos;
			sc_playerEnterSight.MaxHp = pPlayer_i->MaxHp;
			sc_playerEnterSight.Hp = pPlayer_i->Hp;
			if (pPlayer_i->bIsDashing)
			{
				sc_playerEnterSight.vel = Vec2f(0.0f, 0.0f);
				sc_playerEnterSight.dir = Vec2f(0.0f, -1.0f);
			}
			else
			{
				sc_playerEnterSight.vel = pPlayer_i->Vel;
				sc_playerEnterSight.dir = pPlayer_i->Dir;
			}
			pNewPlayer->Send(sc_playerEnterSight, true);

			// 해당 유저가 대쉬를 하고있는 경우 이를 알린다. 
			if (pPlayer_i->bIsDashing)
			{
				SC_Dash sc_dash;
				sc_dash.entityID = pPlayer_i->entityID;
				sc_dash.startPos = pPlayer_i->Pos;
				sc_dash.endPos = pPlayer_i->DashPos;
				sc_dash.speed = pPlayer_i->DashSpeed;
				pNewPlayer->Send(sc_dash, true);
			}
		}

		pLink = pNextLink;
	}

	// 새로 들어온 유저에게 이미 생성된 ProjectileEntity를 알린다. 
	pLink = m_attackList.pHead;
	while (pLink != nullptr)
	{
		Link* pNextLink = pLink->pNext;
		AttackEntity* pAttack = (AttackEntity*)pLink->pItem;

		SC_ProjectileAttackEnterSight sc_packet;
		sc_packet.entityID = pAttack->entityID;
		sc_packet.playerID = pAttack->PlayerEntityID;
		sc_packet.pos = pAttack->Pos;
		sc_packet.vel = pAttack->Vel;
		pNewPlayer->Send(sc_packet, true);

		pLink = pNextLink;
	}

	AppendToList(&m_playerList, &pNewPlayer->link);

	return bRet;
}

bool ArenaRoom::leaveRoom(ConnSession* pSession, eLeaveReason leaveReason, eBroadcastTarget broadcastTarget)
{
	bool bRet = Room::leaveRoom(pSession, leaveReason, broadcastTarget);

	PlayerEntity* pPlayer = (PlayerEntity*)pSession->pRoomSession;
	if (leaveReason != eLeaveReason::RoomDestroyed && pPlayer->IsAlive())
	{
		SC_PlayerLeaveSight sc_playerLeaveSight;
		sc_playerLeaveSight.entityID = pPlayer->entityID;

		Link* pLink = m_sessionList.pHead;
		while (pLink != nullptr)
		{
			Link* pNextLink = pLink->pNext;
			ConnSession* pSession_i = (ConnSession*)pLink->pItem;
			NetConn* pConn_i = pSession_i->pConn;
			pConn_i->Send(sc_playerLeaveSight, true);

			pLink = pNextLink;
		}
	}

	m_entityRegistery.Unregister(pPlayer->entityID);
	if (pPlayer->IsAlive())		// 죽은 상태로 부활하지 않은 player는 playerList에 없다. 
		UnlinkFromList(&m_playerList, &pPlayer->link);
	delete pPlayer;
	pSession->pRoomSession = nullptr;
	
	return bRet;
}

void ArenaRoom::updatePlayerEntity(float dt, PlayerEntity* pPlayer)
{
	if (pPlayer->IsAlive() == false)
		return;

	// delta vector 설정
	Vec2f delta;
	if (pPlayer->bIsDashing)
	{
		Vec2f dashDelta = pPlayer->DashPos - pPlayer->Pos;		// 현재 위치 기준 dashPos에 도달하는 벡터
		Vec2f dashDelta_abs(std::abs(dashDelta.x), std::abs(dashDelta.y));
		if (
			dashDelta_abs.x <= DASH_EPSILON &&
			dashDelta_abs.y <= DASH_EPSILON
		)
		{
			pPlayer->bIsDashing = false;

			return;
		}

		delta = dashDelta.Normalized() * pPlayer->DashSpeed * dt;
		if (
			std::abs(delta.x) > dashDelta_abs.x || 
			std::abs(delta.y) > dashDelta_abs.y
		)
			delta = dashDelta;		// prevent overrun
	}
	else
	{
		delta = pPlayer->Vel * dt;
	}

	if (delta == Vec2f(0.0f, 0.0f))
	{
		assert(isInWall(pPlayer) == nullptr);

		// 플레이어의 위치가 바뀌었을 수 있음으로 현재 위치에서 공격당했는지 확인한다. 
		Aabb2D aabb = pPlayer->GetAabb2D(0.0f);
		Link* pLink = m_attackList.pHead;
		while (pLink != nullptr)
		{
			Link* pNextLink = pLink->pNext;
			AttackEntity* pAttack = (AttackEntity*)pLink->pItem;
			bool bIgnore = pAttack->PlayerEntityID == pPlayer->entityID || pPlayer->bIsDashing;
			if (bIgnore == false)
			{
				Aabb2D attackAabb = pAttack->GetAabb2D(0.0f);
				if (aabb.IsOverlap(attackAabb, true))
				{
					player_vs_projectile_attack(pPlayer, pAttack);
					if (pPlayer->IsAlive() == false)
						break;
				}
			}

			pLink = pNextLink;
		}
	}
	else
	{
		// delta vector를 다 소진시킬 때까지 반복한다. 
		int sweepCount = 0;
		LinkedList ignoreList;
		CollisionInfo info;
		while (delta != Vec2f(0.0f, 0.0f))		// delta가 다 소진될 때 까지 반복한다. 
		{
			if (sweepCount == 2)
				break;

			// m_attackList과 m_wallList를 순회하여 최소값을 찾는다. 
			pPlayer->Sweep(m_attackList, delta, &info);
			pPlayer->Sweep(m_wallList, delta, &info);
			sweepCount++;

			if (info.pSweepEntity == nullptr)
			{
				pPlayer->Apply(info, &delta);
				info.CleanUp();
			}
			else
			{
				eEntity entityType = (eEntity)info.pSweepEntity->entityType;
				if (entityType == eEntity::Attack)
				{
					AttackEntity* pAttack = (AttackEntity*)info.pSweepEntity;
					bool bIgnore = pAttack->PlayerEntityID == pPlayer->entityID || pPlayer->bIsDashing;
					// bool bIgnore = pAttack->playerEntityID == pPlayer->entityID;
					if (bIgnore)	// 자기 자신이 만들어낸 AttackEntity인 경우
					{
						// 이번 프레임에 해당 Attack 객체와 중복 충돌처리 하지 않는다. 
						UnlinkFromList(&m_attackList, &pAttack->link);
						AppendToList(&ignoreList, &pAttack->link);

						info.CleanUp();
					}
					else if (pPlayer->IsAlive())	// 속도 처리하다가 플레이어가 죽을 수도 있다. 
					{
						player_vs_projectile_attack(pPlayer, pAttack);

						break;
					}

					continue;
				}
				else if (entityType == eEntity::Wall)
				{
					assert(info.SweepScale >= 0.0f);
					pPlayer->Apply(info, &delta);
					info.CleanUp();

					// 대쉬 중에는 바로 막힌다. 
					if (pPlayer->bIsDashing)
					{
						pPlayer->bIsDashing = false;
						delta = Vec2f(0.0f, 0.0f);
					}
				}
			}
		}		// end of while (delta != Vec2f(0.0f, 0.0f))
		MergeList(&m_attackList, &ignoreList);		// 무시된 AttackEntity를 돌려놓는다. 	
	}
}

void ArenaRoom::updateAttackEntity(float dt, AttackEntity* pAttack)
{
	LinkedList ignoreList;
	Vec2f delta = pAttack->Vel * dt;
	if (delta == Vec2f(0.0f, 0.0f))
	{
		// 가만히 공중에 떠있어도 공격 가능하도록 처리한다. 
		WallEntity* pWall = isInWall(pAttack);
		if (pWall != nullptr)
		{
			attack_vs_wall(pAttack, pWall);

			return;
		}

		Aabb2D aabb = pAttack->GetAabb2D(0.0f);
		Link* pLink = m_playerList.pHead;
		while (pLink != nullptr)
		{
			Link* pNextLink = pLink->pNext;
			PlayerEntity* pPlayer = (PlayerEntity*)pLink->pItem;
			bool bIgnore = pAttack->PlayerEntityID == pPlayer->entityID || pPlayer->bIsDashing;
			if (bIgnore == false)
			{
				Aabb2D playerAabb = pPlayer->GetAabb2D(0.0f);
				if (aabb.IsOverlap(playerAabb, true))
				{
					player_vs_projectile_attack(pPlayer, pAttack);

					return;
				}
			}

			pLink = pNextLink;
		}
	}
	else
	{
		// delta를 다 소진시킨다. 
		CollisionInfo info;
		while (delta != Vec2f(0.0f, 0.0f))		// delta가 다 소진될 때 까지 반복한다. 
		{
			// m_attackList과 m_wallList를 순회하여 최소값을 찾는다. 
			pAttack->Sweep(m_playerList, delta, &info);
			pAttack->Sweep(m_wallList, delta, &info);

			// 최소값을 적용한다. 
			if (info.pSweepEntity == nullptr)
			{
				pAttack->Apply(info, &delta);
				info.CleanUp();
			}
			else
			{
				eEntity entityType = (eEntity)info.pSweepEntity->entityType;
				assert(entityType != eEntity::Attack);
				if (entityType == eEntity::Wall)
				{
					WallEntity* pWall = (WallEntity*)info.pSweepEntity;
					attack_vs_wall(pAttack, pWall);

					break;
				}
				else if (entityType == eEntity::Player)
				{
					PlayerEntity* pPlayer = (PlayerEntity*)info.pSweepEntity;
					bool bIgnore =
						pAttack->PlayerEntityID == pPlayer->entityID ||
						pPlayer->IsAlive() == false ||
						pPlayer->bIsDashing;
					// bool bIgnore = pAttack->playerEntityID == pPlayer->entityID;
					if (bIgnore)
					{
						// 이번 프레임에 해당 Attack 객체와 중복 충돌처리 하지 않는다. 
						UnlinkFromList(&m_playerList, &pPlayer->link);
						AppendToList(&ignoreList, &pPlayer->link);

						info.CleanUp();
					}
					else
					{
						player_vs_projectile_attack(pPlayer, pAttack);

						break;
					}
				}

				info.CleanUp();
			}
		}		// end of while (delta != Vec2f(0.0f, 0.0f))

		MergeList(&m_playerList, &ignoreList);		// 무시된 PlayerEntity를 돌려놓는다. 
	}
}

void ArenaRoom::attack_vs_wall(AttackEntity* pAttack, WallEntity* pWall)
{
	arena_room::SC_ProjectileDamage packet;
	packet.projectileID = pAttack->entityID;
	packet.targetID = INVALID_ENTITY_ID;
	packet.damage = 0;
	Link* pLink = m_sessionList.pHead;
	while (pLink != nullptr)
	{
		Link* pNextLink = pLink->pNext;
		ConnSession* pSession_i = (ConnSession*)pLink->pItem;
		PlayerEntity* pPlayer_i = (PlayerEntity*)pSession_i->pRoomSession;
		pPlayer_i->Send(packet, true);

		pLink = pNextLink;
	}

	UnlinkFromList(&m_attackList, &pAttack->link);
	m_entityRegistery.Unregister(pAttack->entityID);
	delete pAttack;
}

void ArenaRoom::player_vs_projectile_attack(PlayerEntity* pDamagedPlayer, AttackEntity* pAttack)
{
#ifndef _DEBUG_
	assert(pDamagedPlayer->IsAlive());
#endif // !_DEBUG_

    // 데미지 감소 처리
	pDamagedPlayer->Hp -= pAttack->Damage;
	pDamagedPlayer->Hp = std::max(0, pDamagedPlayer->Hp);
	bool isDead = pDamagedPlayer->Hp == 0;

	// 만약 사망한 경우 despawn 처리한다. 
	arena_room::SC_DespawnRemotePlayer sc_despawnRemote;
	sc_despawnRemote.entityID = pDamagedPlayer->entityID;
	sc_despawnRemote.killerNickname = L"";
	if (isDead)
	{
		pDamagedPlayer->Vel = Vec2f(0.0f, 0.0f);

		PlayerEntity* pAttackPlayer = (PlayerEntity*)m_entityRegistery.Get(pAttack->PlayerEntityID);
		if (pAttackPlayer != nullptr)
		{
			if (pAttack->PlayerRegisterCount == pAttackPlayer->RegisterCount)
				sc_despawnRemote.killerNickname = pAttackPlayer->GetNickname();
		}
	}

	// 대미지 패킷 전송
	arena_room::SC_ProjectileDamage sc_projectileDamage;
	sc_projectileDamage.projectileID = pAttack->entityID;
	sc_projectileDamage.targetID = pDamagedPlayer->entityID;
	sc_projectileDamage.damage = pAttack->Damage;
	sc_projectileDamage.hp = pDamagedPlayer->Hp;

	ConnSession* pSession = pDamagedPlayer->pConnSession;

	Link* pLink = m_sessionList.pHead;
	while (pLink != nullptr)
	{
		Link* pNextLink = pLink->pNext;
		ConnSession* pSession_i = (ConnSession*)pLink->pItem;
		pSession_i->Send(sc_projectileDamage, true);

		if (isDead)
		{
			if (pSession == pSession_i)
			{
				arena_room::SC_DespawnLocalPlayer sc_despawnLocal;
				sc_despawnLocal.entityID = pDamagedPlayer->entityID;
				pSession_i->Send(sc_despawnLocal, true);
			}
			else
			{
				pSession_i->Send(sc_despawnRemote, true);
			}
		}

		pLink = pNextLink;
	}

	// 죽은 유저는 업데이트할 필요가 없고 상호작용에 참가하지 않음으로, 
	// 부활할 때 까지 playerList에서 빼둔다. 
	if (isDead)
		UnlinkFromList(&m_playerList, &pDamagedPlayer->link);

	UnlinkFromList(&m_attackList, &pAttack->link);
	m_entityRegistery.Unregister(pAttack->entityID);
	delete pAttack;
}

void ArenaRoom::player_vs_melee_attack(PlayerEntity* pAttackPlayer, PlayerEntity* pDamagedPlayer, int damage)
{
#ifndef _DEBUG_
	assert(pAttackPlayer->IsAlive());
	assert(pDamagedPlayer->IsAlive());
#endif // !_DEBUG_

    // 데미지 처리
	pDamagedPlayer->Hp -= damage;
	pDamagedPlayer->Hp = std::max(0, pDamagedPlayer->Hp);
	bool isDead = pDamagedPlayer->Hp == 0;
	if (isDead)
		pDamagedPlayer->Vel = Vec2f(0.0f, 0.0f);

	// 사망한 경우 전송되는 패킷
	arena_room::SC_DespawnRemotePlayer sc_despawnRemote;
	sc_despawnRemote.entityID = pDamagedPlayer->entityID;
	sc_despawnRemote.killerNickname = pAttackPlayer->GetNickname();

	// 데미지 패킷
	arena_room::SC_MeleeDamage sc_projectileDamage;
	sc_projectileDamage.TargetID = pDamagedPlayer->entityID;
	sc_projectileDamage.Damage = damage;
	sc_projectileDamage.Hp = pDamagedPlayer->Hp;
	
	// 패킷 전송
	ConnSession* pDamagedSession = pDamagedPlayer->pConnSession;
	Link* pLink = m_sessionList.pHead;
	while (pLink != nullptr)
	{
		Link* pNextLink = pLink->pNext;
		ConnSession* pSession_i = (ConnSession*)pLink->pItem;
		pSession_i->Send(sc_projectileDamage, true);

		if (isDead)
		{
			if (pDamagedSession == pSession_i)
			{
				arena_room::SC_DespawnLocalPlayer sc_despawnLocal;
				sc_despawnLocal.entityID = pDamagedPlayer->entityID;
				pSession_i->Send(sc_despawnLocal, true);
			}
			else
			{
				pSession_i->Send(sc_despawnRemote, true);
			}
		}

		pLink = pNextLink;
	}

	// 죽은 유저는 업데이트할 필요가 없고 상호작용에 참가하지 않음으로, 
	// 부활할 때 까지 playerList에서 빼둔다. 
	if (isDead)
		UnlinkFromList(&m_playerList, &pDamagedPlayer->link);
}

ArenaRoom::PlayerEntity* ArenaRoom::getPlayerEntity(NetConn* pConn)
{
	assert(pConn != nullptr);
	ConnSession* pSession = GetConnSession(pConn);
	assert(pSession != nullptr);
	PlayerEntity* pRet = (PlayerEntity*)pSession->pRoomSession;
	assert(pRet->entityType == (EntityType)eEntity::Player);

	return pRet;
}

ArenaRoom::PlayerEntity* ArenaRoom::getPlayerEntity(ConnSession* pSession)
{
	assert(pSession != nullptr);
	PlayerEntity* pRet = (PlayerEntity*)pSession->pRoomSession;
	assert(pRet->entityType == (EntityType)eEntity::Player);

	return pRet;
}

ArenaRoom::WallEntity* ArenaRoom::isInWall(SweepEntity* pSweepEntity)
{
	Aabb2D playerAabb = pSweepEntity->GetAabb2D(0.0f);

	Link* pLink = m_wallList.pHead;
	while (pLink != nullptr)
	{
		Link* pNextLink = pLink->pNext;
		WallEntity* pWall = (WallEntity*)pLink->pItem;
		assert(pWall->entityType == (EntityType)eEntity::Wall);
		Aabb2D wallAabb = pWall->GetAabb2D(0.0f);
		if (playerAabb.IsOverlap(wallAabb, true))
			return pWall;

		pLink = pNextLink;
	}

	return nullptr;
}

Vec2f ArenaRoom::getSpawnPos()
{
	Vec2f ret = m_spawnPoses[m_spawnPosIndex % ARRAYSIZE(m_spawnPoses)];
	m_spawnPosIndex++;

	return ret;
}
