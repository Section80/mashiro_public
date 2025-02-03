#pragma once

#include "Packable.h"

enum class eMessageType : BYTE
{
	None, 
	Event, 
	Job, 
	Packet

};

class Event : public IPackable
{
public:
	Event(const PackableInfo* pInfo);

	bool Pack(IWriter* pWriter) override final;
	bool Unpack(IReader* pReader) override final;

	PackableType GetType();

protected:
	virtual bool packField(IWriter* pWriter);
	virtual bool unpackField(IReader* pReader);

private:
	const PackableInfo* m_pInfo;

};

class Job : public Event
{
public:
	Job(const PackableInfo* pInfo);

	virtual bool Execute() = 0;

protected:
	virtual bool packField(IWriter* pWriter) override;
	virtual bool unpackField(IReader* pReader) override;

};

class ReturnFalseJob : public Job
{
public:
	ReturnFalseJob();

	bool Execute() override;

};

using Opcode = PackableType;
class Packet : public IPackable
{
public:
	Packet(PackableType opcode);

	bool Pack(IWriter* pWriter) final override;
	bool Unpack(IReader* pReader) final override;

	PackableType GetOpcode();

protected:
	virtual bool packField(IWriter* pWriter);
	virtual bool unpackField(IReader* pReader);

private:
	PackableType m_opcode;

};
