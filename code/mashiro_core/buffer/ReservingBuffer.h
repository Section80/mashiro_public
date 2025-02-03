#pragma once

#include "buffer/RangedBuffer.h"

class ReservingBuffer final : public IReader
{
public:
	ReservingBuffer();									// null
	ReservingBuffer(SIZE_T bufferSize);					// allocate internal buffer
	ReservingBuffer(void* pBuffer, SIZE_T bufferSize);	// use outer pBuffer
	ReservingBuffer(ReservingBuffer& other) = delete;
	ReservingBuffer(ReservingBuffer&& other) = delete;

	virtual ~ReservingBuffer();							// CleanUp()

	bool Init(SIZE_T bufferSize);						// allocate internal buffer
	bool Init(void* pBuffer, SIZE_T bufferSize);		// use outer pBuffer
	bool IsNull();
	void CleanUp();

public:
	RangedWriter ReserveWrite(SIZE_T reserveSize);

	SIZE_T GetReadableSize() override;
	void* ReadRaw(SIZE_T nBytes, bool bAddIndex) override;

	void Rewind(bool bRead, bool bWrite);

	SIZE_T GetReadIndex();
	SIZE_T GetWriteIndex();

private:
	bool		m_isOwnBuffer;
	BYTE*		m_pBuffer;
	SIZE_T		m_bufferSize;

	SRWLOCK		m_writeIndexLock;
	SIZE_T		m_readIndex;
	SIZE_T		m_writeIndex;

};