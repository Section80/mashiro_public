#pragma once

#include "Packable.h"

class RangedReader final : public IReader
{
public:
	RangedReader();
	RangedReader(void* pBuffer, SIZE_T bufferSize);
	RangedReader(RangedReader& other) = delete;
	RangedReader(RangedReader&& other) = delete;

	void Init(void* pBuffer, SIZE_T bufferSize);
	bool IsNull();
	void CleanUp();

	SIZE_T GetReadableSize() override;
	void* ReadRaw(SIZE_T nBytes, bool bAddIndex) override;

	void Rewind();

private:
	BYTE*	m_pBuffer;
	SIZE_T	m_bufferSize;
	SIZE_T	m_readIndex;

};

class RangedWriter final : public IWriter
{
public:
	RangedWriter();
	RangedWriter(void* pBuffer, SIZE_T bufferSize);

	void Init(void* pBuffer, SIZE_T bufferSize);
	bool IsNull();
	void CleanUp();

	void* WriteRaw(const void* pSource, SIZE_T nBytes) override;
	SIZE_T GetWritableSize() override;

	SIZE_T GetWrittenSize();

	void Rewind();

private:
	BYTE* m_pBuffer;
	SIZE_T m_bufferSize;

	SIZE_T m_writeIndex;

};