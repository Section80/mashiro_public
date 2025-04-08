#pragma once
  
#include "MashiroGlobal.h"

class IReader
{
public:
	virtual SIZE_T GetReadableSize() = 0;

	virtual void* ReadRaw(SIZE_T nBytes, bool bAddIndex) = 0;

public:		// no overloading!
	void* PeekPointer(void** pPointer);
	void* PeekBool(bool* pOut);
	void* PeekChar(char* pOut);
	void* PeekByte(unsigned char* pOut);
	void* PeekShort(short* pOut);
	void* PeekUShort(unsigned short* pOut);
	void* PeekInt(int* pOut);
	void* PeekUInt(unsigned int* pOut);
	void* PeekLongLong(long long* pOut);
	void* PeekULongLong(unsigned long long* pOut);
	void* PeekFloat(float* pOut);
	void* PeekDouble(double* pOut);
	// void* Peek(std::string_view* pOut);		// not supported
	// void* Peek(std::wstring_view* pOut);		// not supported

	void* ReadPointer(void** pPointer);
	void* ReadBool(bool* pOut);
	void* ReadChar(char* pOut);
	void* ReadByte(unsigned char* pOut);
	void* ReadShort(short* pOut);
	void* ReadUShort(unsigned short* pOut);
	void* ReadInt(int* pOut);
	void* ReadUInt(unsigned int* pOut);
	void* ReadLongLong(long long* pOut);
	void* ReadULongLong(unsigned long long* pOut);
	void* ReadFloat(float* pOut);
	void* ReadDouble(double* pOut);
	void* ReadString(std::string_view* pOut);
	void* ReadWString(std::wstring_view* pOut);						// ntoh
	void* ReadWString(USHORT* pOutCount, WCHAR** ppOutBuffer);		// ntoh

};

class PointReader : public IReader
{
public:
	PointReader(void* pPoint);

	SIZE_T	GetReadableSize() override;
	void*	ReadRaw(SIZE_T nBytes, bool bAddIndex) override;

private:
	BYTE*	m_pPoint;
	SIZE_T	m_readIndex;

};

class IWriter
{
public:
	virtual SIZE_T GetWritableSize() = 0;

	virtual void* WriteRaw(const void* pSource, SIZE_T nBytes) = 0;

	void* WritePointer(void* value);
	void* WriteBool(bool value);
	void* WriteChar(char value);
	void* WriteByte(unsigned char value);
	void* WriteShort(short value);
	void* WriteUShort(unsigned short value);
	void* WriteInt(int value);
	void* WriteUInt(unsigned int value);
	void* WriteLongLong(long long value);
	void* WriteULongLong(unsigned long long value);
	void* WriteFloat(float value);
	void* WriteDouble(double value);
	void* WriteString(const std::string_view value);
	virtual void* WriteWString(const std::wstring_view value);		// default: hton
	virtual void* WriteWString(USHORT count, WCHAR* pBuffer);

};

// copy�� ���� �ʰ� ����� ���. 
class SizeCalculator final : public IWriter
{
public:
	SizeCalculator();

	virtual SIZE_T	GetWritableSize() override;
	virtual void*	WriteRaw(const void* pSource, SIZE_T nBytes) override;
	SIZE_T			GetWrittenSize();

	void* WriteWString(const std::wstring_view value) override;
	void* WriteWString(USHORT count, WCHAR* pBuffer) override;

private:
	SIZE_T m_nWrittenBytes;

};

using	PackableType = unsigned short;
extern	PackableType INVALID_PACKABLE;

class IPackable
{
public:
	// calls placement new ctor on buffer
	using InitFunc = IPackable * (*)(void* pBuffer, SIZE_T bufferSize);

public:
	IPackable();				// do nothing
	virtual ~IPackable();		// do nothing

	virtual bool Pack(IWriter* pWriter) = 0;
	virtual bool Unpack(IReader* pReader) = 0;

	SIZE_T ComputePackedSize();

public:
	template <typename Packable>
		requires(std::is_base_of_v<IPackable, Packable>)
	static PackableType GetSetPackableType(bool bSet, PackableType value, const char* file, const char* func, int line)
	{
		volatile static PackableType s_type = INVALID_PACKABLE;

		if (bSet)	// set
		{
			if (value == INVALID_PACKABLE)
			{
				LogError(file, func, line, "Can't set IPackable's type value to INVALID_TYPE");
				DebugBreak();

				return s_type;
			}

			if (s_type != INVALID_PACKABLE)
			{
				LogError(file, func, line, "Try to overwrite IPackable's type value: %u", value);
				DebugBreak();

				return s_type;
			}

			s_type = value;
		}

		return s_type;
	}

};

#define REGISTER_PACKABLE_TYPE(Packable, value) (IPackable::GetSetPackableType<Packable>(true, (PackableType)value, __FILE__, __FUNCTION__, __LINE__))
#define GET_PACKABLE_TYPE(Packable) (IPackable::GetSetPackableType<Packable>(false, INVALID_PACKABLE, __FILE__, __FUNCTION__, __LINE__))

struct PackableInfo
{
	IPackable::InitFunc initFunc = nullptr;
	PackableType type = INVALID_PACKABLE;
	SIZE_T unpackedSize = -1;

};

template <typename Packable>
//	requires(std::is_base_of_v<IPackable, Packable>)
const PackableInfo* GetPackableInfo()
{
	PackableType packableType = GET_PACKABLE_TYPE(Packable);
	// if (packableType == INVALID_PACKABLE)		// not registered type
	//	return nullptr;

	static PackableInfo s_info =
	{
		.initFunc = [](void* pBuffer, SIZE_T bufferSize)
		{
			assert(pBuffer != nullptr);

			IPackable* pRet = (IPackable*)std::align(alignof(Packable), sizeof(Packable), pBuffer, bufferSize);
			if (pRet == nullptr)		// buffer too small
				return pRet;

			pRet = new (pRet)Packable();

			return pRet;
		},
		.type = packableType,
		.unpackedSize = sizeof(Packable)
	};

	return &s_info;
}

template <typename Packable>
	requires(std::is_base_of_v<IPackable, Packable>)
bool IsIt(PackableType it)
{
	bool bRet = GET_PACKABLE_TYPE(Packable) == it;
	
	return bRet;
}
