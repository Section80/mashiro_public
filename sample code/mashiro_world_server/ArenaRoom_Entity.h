#ifndef _ARENA_ROOM_ENTITY_
#define _ARENA_ROOM_ENTITY_

#include "./ArenaRoom.h"

class ArenaRoom::PlayerEntity : public SweepEntity
{
public:
	PlayerEntity(ConnSession* pConnSession, int hp, Vec2f pos);

	bool Send(Packet& packet, bool bNow);

	std::wstring_view GetNickname();
	PlayerCharacter::eClass GetCharacterClass();

	bool IsAlive();		// check if Hp > 0

public:
	ConnSession* pConnSession;

	Vec2f Vel;
	Vec2f Dir;

	bool bIsDashing;
	Vec2f DashPos;

	float DashSpeed;

	int MaxHp;
	int Hp;
	LARGE_INTEGER LastPortionTime;

	ULONGLONG RegisterCount;

};

class ArenaRoom::AttackEntity : public SweepEntity
{
public:
	AttackEntity(EntityID playerEntityID, Vec2f pos, Vec2f size, Vec2f vel);

public:
	EntityID PlayerEntityID;		// 이 AttackEntity를 발생시킨 PlayerEntity의 ID
	ULONGLONG PlayerRegisterCount;

	Vec2f Vel;
	
	int Damage;

};

class ArenaRoom::WallEntity : public SweepEntity
{
public:
	WallEntity();
	WallEntity(eCenter center, Vec2f pos, Vec2f size);
	WallEntity(Vec2f pos, Vec2f size);		// eCenter::BottomCenter

	void Init(eCenter center, Vec2f pos, Vec2f size);
	void Init(Vec2f pos, Vec2f size);

public:
	Link link;

};

#endif
