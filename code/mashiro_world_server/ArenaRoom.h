#pragma once

#ifndef _ARENA_ROOM_
#define _ARENA_ROOM_

#include "../Room.h"
#include "../SweepAabb.h"
#include "./ArenaRoom_PacketDef.h"

class ArenaRoom : public Room
{
public:
	static constexpr int PLAYER_HP = 100;

	// static constexpr float DASH_SPEED = 16.0f;
	static constexpr float DASH_EPSILON = 0.002f;			// DASH_EPSILON은 Aabb2D.EPSILON보다 유의미하게 커야한다. 
	// static constexpr float DASH_DISTNACE = 6.0f;
	enum class eEntity : EntityType
	{
		Wall = 0,
		Player = 1, 
		Attack = 2, 

	};

	class PlayerEntity;
	class AttackEntity;
	class WallEntity;

public:
	ArenaRoom(ConnSession* pMasterSession, int capacity);
	virtual ~ArenaRoom() override;

	bool Update(float dt) override;
	bool HandlePacket(NetConn* pSender, Packet& packet, bool bLog) override;

protected:	// packet handlers
	bool handleStartMove(NetConn* pSender, CS_StartMove& packet, bool bLog);
	bool handleStopMove(NetConn* pSender, CS_StopMove& packet, bool bLog);
	bool handleDash(NetConn* pSender, arena_room::CS_Dash& packet, bool bLog);
	bool handleMeleeAttack(NetConn* pSender, arena_room::CS_MeleeAttack& packet, bool bLog);
	bool handleProjectileAttack(NetConn* pSender, arena_room::CS_ProjectileAttack& packet, bool bLog);
	bool handleRoomChat(NetConn* pSender, CS_RoomChat& packet, bool bLog);
	bool handleSpawnLocalPlayer(NetConn* pSender, arena_room::CS_SpawnLocalPlayer& packet, bool bLog);
	bool handleHpPortion(NetConn* pSender, arena_room::CS_HpPortion& packet, bool bLog);

protected:
	bool enterRoom(ConnSession* pSession, eBroadcastTarget broadcastTarget) override;
	bool leaveRoom(ConnSession* pSession, eLeaveReason leaveReason, eBroadcastTarget broadcastTarget) override;

protected:	
	void updatePlayerEntity(float dt, PlayerEntity* pPlayer);
	void updateAttackEntity(float dt, AttackEntity* pAttack);

protected:	// collision callback
	void attack_vs_wall(AttackEntity* pAttack, WallEntity* pWall);
	void player_vs_projectile_attack(PlayerEntity* pPlayer, AttackEntity* pAttack);
	void player_vs_melee_attack(PlayerEntity* pPlayer, PlayerEntity* pDamagedPlayer, int damage);

protected:
	PlayerEntity*	getPlayerEntity(NetConn* pConn);
	PlayerEntity*	getPlayerEntity(ConnSession* pSession);
	
	WallEntity*		isInWall(SweepEntity* pSweepEntity);

	Vec2f			getSpawnPos();

public:
	EntityRegistery		m_entityRegistery;

	LinkedList			m_playerList;
	LinkedList			m_wallList;
	LinkedList			m_attackList;

	int					m_spawnPosIndex;
	Vec2f				m_spawnPoses[5];

};

#endif // !_ARENA_ROOM_
