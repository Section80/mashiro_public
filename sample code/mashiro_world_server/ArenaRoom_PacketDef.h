#ifndef _ARENA_ROOM_PACKET_
#define _ARENA_ROOM_PACKET_

#include "../../PacketDef.h"

namespace arena_room
{
	class SC_PlayerEnterSight : public Packet
	{
	public:
		SC_PlayerEnterSight()
			: Packet(GET_PACKABLE_TYPE(SC_PlayerEnterSight))
			, entityID(INVALID_ENTITY_ID)
			, nickname()
			, characterClass(PlayerCharacter::eClass::None)
			, pos()
			, vel()
			, dir()
			, MaxHp(0)
			, Hp(0)
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pWriter->WriteUShort(entityID) == nullptr)
				return false;
			if (pWriter->WriteWString(nickname) == nullptr)
				return false;
			if (pWriter->WriteByte((BYTE)characterClass) == nullptr)
				return false;
			if (pos.Pack(pWriter) == false)
				return false;
			if (vel.Pack(pWriter) == false)
				return false;
			if (dir.Pack(pWriter) == false)
				return false;
			if (pWriter->WriteInt(MaxHp) == nullptr)
				return false;
			if (pWriter->WriteInt(Hp) == nullptr)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			return false;
		}

	public:
		EntityID					entityID;
		std::wstring_view			nickname;
		PlayerCharacter::eClass		characterClass;

		Vec2f	pos;
		Vec2f	vel;
		Vec2f	dir;

		int MaxHp;
		int Hp;

	};

	class SC_PlayerLeaveSight : public Packet
	{
	public:
		SC_PlayerLeaveSight()
			: Packet(GET_PACKABLE_TYPE(SC_PlayerLeaveSight))
			, entityID(INVALID_ENTITY_ID)
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pWriter->WriteUShort(entityID) == nullptr)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			return false;
		}

	public:
		EntityID entityID;

	};

	class CS_SpawnLocalPlayer : public Packet
	{
	public:
		CS_SpawnLocalPlayer()
			: Packet(GET_PACKABLE_TYPE(CS_SpawnLocalPlayer))
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			return false;
		}

		bool unpackField(IReader* pReader) override
		{
			return true;
		}

	};

	class SC_SpawnLocalPlayer : public Packet
	{
	public:
		SC_SpawnLocalPlayer()
			: Packet(GET_PACKABLE_TYPE(SC_SpawnLocalPlayer))
			, entityID(INVALID_ENTITY_ID)
			, pos()
			, dir()
			, MaxHp(0)
			, Hp(0)
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pWriter->WriteUShort(entityID) == nullptr)
				return false;
			if (pos.Pack(pWriter) == false)
				return false;
			if (dir.Pack(pWriter) == false)
				return false;
			if (pWriter->WriteInt(MaxHp) == nullptr)
				return false;
			if (pWriter->WriteInt(Hp) == nullptr)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			return false;
		}

	public:
		EntityID	entityID;
		Vec2f		pos;
		Vec2f		dir;

		int MaxHp;
		int Hp;

	};

	class SC_DespawnLocalPlayer : public Packet
	{
	public:
		SC_DespawnLocalPlayer()
			: Packet(GET_PACKABLE_TYPE(SC_DespawnLocalPlayer))
			, entityID(INVALID_ENTITY_ID)
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pWriter->WriteUShort(entityID) == nullptr)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			if (pReader->ReadUShort(&entityID))
				return false;

			return true;
		}

	public:
		EntityID entityID;

	};

	class SC_SpawnRemotePlayer : public Packet
	{
	public:
		SC_SpawnRemotePlayer()
			: Packet(GET_PACKABLE_TYPE(SC_SpawnRemotePlayer))
			, entityID(INVALID_ENTITY_ID)
			, nickname()
			, characterClass(PlayerCharacter::eClass::None)
			, pos()
			, dir()
			, MaxHp(0)
			, Hp(0)
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pWriter->WriteUShort(entityID) == nullptr)
				return false;
			if (pWriter->WriteWString(nickname) == nullptr)
				return false;
			if (pWriter->WriteByte((BYTE)characterClass) == nullptr)
				return false;
			if (pos.Pack(pWriter) == false)
				return false;
			if (dir.Pack(pWriter) == false)
				return false;
			if (pWriter->WriteInt(MaxHp) == nullptr)
				return false;
			if (pWriter->WriteInt(Hp) == nullptr)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			return false;
		}

	public:
		EntityID					entityID;
		std::wstring_view			nickname;
		PlayerCharacter::eClass		characterClass;

		Vec2f						pos;
		Vec2f						dir;

		int MaxHp;
		int Hp;

	};

	class SC_DespawnRemotePlayer : public Packet
	{
	public:
		SC_DespawnRemotePlayer()
			: Packet(GET_PACKABLE_TYPE(SC_DespawnRemotePlayer))
			, entityID(INVALID_ENTITY_ID)
			, killerNickname(L"")
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pWriter->WriteUShort(entityID) == nullptr)
				return false;
			if (pWriter->WriteWString(killerNickname) == nullptr)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			if (pReader->ReadUShort(&entityID))
				return false;
			if (pReader->ReadWString(&killerNickname))
				return false;

			return true;
		}

	public:
		EntityID			entityID;
		std::wstring_view	killerNickname;

	};

	class SC_NpcEnterSight : public Packet
	{
	public:
		SC_NpcEnterSight()
			: Packet(GET_PACKABLE_TYPE(SC_NpcEnterSight))
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			return true;
		}

	public:

	};

	class SC_NpcLeaveSight : public Packet
	{
	public:
		SC_NpcLeaveSight()
			: Packet(GET_PACKABLE_TYPE(SC_NpcLeaveSight))
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			return true;
		}

	public:

	};

	class SC_SpawnNpc : public Packet
	{
	public:
		SC_SpawnNpc()
			: Packet(GET_PACKABLE_TYPE(SC_SpawnNpc))
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			return true;
		}

	public:

	};

	class SC_DespawnNpc : public Packet
	{
	public:
		SC_DespawnNpc()
			: Packet(GET_PACKABLE_TYPE(SC_DespawnNpc))
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			return true;
		}

	public:

	};

	class CS_Dash : public Packet
	{
	public:
		CS_Dash()
			: Packet(GET_PACKABLE_TYPE(CS_Dash))
			, pos{}
			, dir{}
			, DashDistance(0.0f)
			, DashSpeed(0.0f)
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pos.Pack(pWriter) == false)
				return false;
			if (dir.Pack(pWriter) == false)
				return false;
			if (pWriter->WriteFloat(DashDistance) == nullptr)
				return false;
			if (pWriter->WriteFloat(DashSpeed) == nullptr)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			if (pos.Unpack(pReader) == false)
				return false;
			if (dir.Unpack(pReader) == false)
				return false;
			if (pReader->ReadFloat(&DashDistance) == nullptr)
				return false;
			if (pReader->ReadFloat(&DashSpeed) == nullptr)
				return false;

			return true;
		}

	public:
		Vec2f	pos;
		Vec2f	dir;
		float	DashDistance;
		float	DashSpeed;


	};

	class SC_Dash : public Packet
	{
	public:
		SC_Dash()
			: Packet(GET_PACKABLE_TYPE(SC_Dash))
			, entityID(INVALID_ENTITY_ID)
			, speed(0.0f)
			, startPos()
			, endPos()
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pWriter->WriteUShort(entityID) == nullptr)
				return false;
			if (pWriter->WriteFloat(speed) == nullptr)
				return false;
			if (startPos.Pack(pWriter) == false)
				return false;
			if (endPos.Pack(pWriter) == false)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			if (pReader->ReadUShort(&entityID) == nullptr)
				return false;
			if (pReader->ReadFloat(&speed) == nullptr)
				return false;
			if (startPos.Unpack(pReader) == false)
				return false;
			if (endPos.Unpack(pReader) == false)
				return false;

			return true;
		}

	public:
		EntityID	entityID;
		float		speed;
		Vec2f		startPos;
		Vec2f		endPos;

	};

	class CS_MeleeAttack : public Packet
	{
	public:
		CS_MeleeAttack()
			: Packet(GET_PACKABLE_TYPE(CS_MeleeAttack))
			, Pos{}
			, Dir{}
			, Speed(0)
			, Damage(0)
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (Pos.Pack(pWriter) == false)
				return false;
			if (Dir.Pack(pWriter) == false)
				return false;
			if (pWriter->WriteFloat(Speed) == nullptr)
				return false;
			if (pWriter->WriteInt(Damage) == nullptr)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			if (Pos.Unpack(pReader) == false)
				return false;
			if (Dir.Unpack(pReader) == false)
				return false;
			if (pReader->ReadFloat(&Speed) == nullptr)
				return false;
			if (pReader->ReadInt(&Damage) == nullptr)
				return false;

			return true;
		}

	public:
		Vec2f	Pos;
		Vec2f	Dir;

		float	Speed;
		int		Damage;

	};

	class SC_MeleeAttack : public Packet
	{
	public:
		SC_MeleeAttack()
			: Packet(GET_PACKABLE_TYPE(SC_MeleeAttack))
			, EntityID(INVALID_ENTITY_ID)
			, Pos{}
			, Dir{}
			, Speed(0)
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pWriter->WriteUShort(EntityID) == nullptr)
				return false;
			if (Pos.Pack(pWriter) == false)
				return false;
			if (Dir.Pack(pWriter) == false)
				return false;
			if (pWriter->WriteFloat(Speed) == nullptr)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			if (pReader->ReadUShort(&EntityID) == nullptr)
				return false;
			if (Pos.Unpack(pReader) == false)
				return false;
			if (Dir.Unpack(pReader) == false)
				return false;
			if (pReader->ReadFloat(&Speed) == nullptr)
				return false;

			return true;
		}

	public:
		EntityID	EntityID;
		Vec2f		Pos;
		Vec2f		Dir;

		float		Speed;

	};

	class SC_MeleeDamage : public Packet
	{
	public:
		SC_MeleeDamage()
			: Packet(GET_PACKABLE_TYPE(SC_MeleeDamage))
			, TargetID(INVALID_ENTITY_ID)
			, Damage(0)
			, Hp(0)
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pWriter->WriteUShort(TargetID) == nullptr)
				return false;
			if (pWriter->WriteInt(Damage) == nullptr)
				return false;
			if (pWriter->WriteInt(Hp) == nullptr)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			if (pReader->ReadUShort(&TargetID) == nullptr)
				return false;
			if (pReader->ReadInt(&Damage) == nullptr)
				return false;
			if (pReader->ReadInt(&Hp) == nullptr)
				return false;

			return true;
		}

	public:
		EntityID	TargetID;
		int			Damage;
		int			Hp;

	};

	class SC_ProjectileAttackEnterSight : public Packet
	{
	public:
		SC_ProjectileAttackEnterSight()
			: Packet(GET_PACKABLE_TYPE(SC_ProjectileAttackEnterSight))
			, entityID(INVALID_ENTITY_ID)
			, playerID(INVALID_ENTITY_ID)
			, pos{}
			, vel{}
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pWriter->WriteUShort(entityID) == nullptr)
				return false;
			if (pWriter->WriteUShort(playerID) == nullptr)
				return false;
			if (pos.Pack(pWriter) == false)
				return false;
			if (vel.Pack(pWriter) == false)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			if (pReader->ReadUShort(&entityID) == nullptr)
				return false;
			if (pReader->ReadUShort(&playerID) == nullptr)
				return false;
			if (pos.Unpack(pReader) == false)
				return false;
			if (vel.Unpack(pReader) == false)
				return false;

			return true;
		}

	public:
		EntityID	entityID;
		EntityID	playerID;

		Vec2f		pos;
		Vec2f		vel;

	};

	class CS_ProjectileAttack : public Packet
	{
	public:
		CS_ProjectileAttack()
			: Packet(GET_PACKABLE_TYPE(CS_ProjectileAttack))
			, pos{}
			, dir{}
			, Damage(0)
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pos.Pack(pWriter) == false)
				return false;
			if (dir.Pack(pWriter) == false)
				return false;
			if (pWriter->WriteInt(Damage) == nullptr)
				return false;
			if (pWriter->WriteFloat(speed) == nullptr)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			if (pos.Unpack(pReader) == false)
				return false;
			if (dir.Unpack(pReader) == false)
				return false;
			if (pReader->ReadInt(&Damage) == nullptr)
				return false;
			if (pReader->ReadFloat(&speed) == nullptr)
				return false;

			return true;
		}

	public:
		Vec2f pos;
		Vec2f dir;
		float speed;

		int Damage;

	};

	class SC_ProjectileAttack : public Packet
	{
	public:
		SC_ProjectileAttack()
			: Packet(GET_PACKABLE_TYPE(SC_ProjectileAttack))
			, entityID(INVALID_ENTITY_ID)
			, playerID(INVALID_ENTITY_ID)
			, pos{}
			, vel{}
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pWriter->WriteUShort(entityID) == nullptr)
				return false;
			if (pWriter->WriteUShort(playerID) == nullptr)
				return false;
			if (pos.Pack(pWriter) == false)
				return false;
			if (vel.Pack(pWriter) == false)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			if (pReader->ReadUShort(&entityID) == nullptr)
				return false;
			if (pReader->ReadUShort(&playerID) == nullptr)
				return false;
			if (pos.Unpack(pReader) == false)
				return false;
			if (vel.Unpack(pReader) == false)
				return false;

			return true;
		}

	public:
		EntityID	entityID;
		EntityID	playerID;

		Vec2f		pos;
		Vec2f		vel;

	};

	class SC_ProjectileDamage : public Packet
	{
	public:
		SC_ProjectileDamage()
			: Packet(GET_PACKABLE_TYPE(SC_ProjectileDamage))
			, projectileID(INVALID_ENTITY_ID)
			, targetID(INVALID_ENTITY_ID)
			, damage(0)
			, hp(0)
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pWriter->WriteUShort(projectileID) == nullptr)
				return false;
			if (pWriter->WriteUShort(targetID) == nullptr)
				return false;
			if (pWriter->WriteInt(damage) == nullptr)
				return false;
			if (pWriter->WriteInt(hp) == nullptr)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			if (pReader->ReadUShort(&projectileID) == nullptr)
				return false;
			if (pReader->ReadUShort(&targetID) == nullptr)
				return false;
			if (pReader->ReadInt(&damage) == nullptr)
				return false;
			if (pReader->ReadInt(&hp) == nullptr)
				return false;

			return true;
		}

	public:
		EntityID projectileID;
		EntityID targetID;
		int damage;
		int hp;

	};

	class CS_HpPortion : public Packet
	{
	public:
		CS_HpPortion()
			: Packet(GET_PACKABLE_TYPE(CS_HpPortion))
			, Amount(0)
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pWriter->WriteInt(Amount) == nullptr)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			if (pReader->ReadInt(&Amount) == nullptr)
				return false;

			return true;
		}

	public:
		int Amount;

	};

	class SC_HpPortion : public Packet
	{
	public:
		SC_HpPortion()
			: Packet(GET_PACKABLE_TYPE(SC_HpPortion))
			, EntityID(INVALID_ENTITY_ID)
			, Amount(0)
			, Hp(0)
		{}

	protected:
		bool packField(IWriter* pWriter) override
		{
			if (pWriter->WriteUShort(EntityID) == nullptr)
				return false;
			if (pWriter->WriteInt(Amount) == nullptr)
				return false;
			if (pWriter->WriteInt(Hp) == nullptr)
				return false;

			return true;
		}

		bool unpackField(IReader* pReader) override
		{
			if (pReader->ReadUShort(&EntityID) == nullptr)
				return false;
			if (pReader->ReadInt(&Amount) == nullptr)
				return false;
			if (pReader->ReadInt(&Hp) == nullptr)
				return false;

			return true;
		}

	public:
		EntityID	EntityID;
		int			Amount;
		int			Hp;

	};

}		/* end of namespace arena_room */

#endif // !_ARENA_ROOM_PACKET_
