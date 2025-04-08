#include "network/Overlapped.h"

using namespace overlapped;

SIZE_T overlapped::Min(SIZE_T a, SIZE_T b)
{
	return (a < b) ? a : b;
}

SIZE_T overlapped::Max(SIZE_T a, SIZE_T b)
{
	return (a > b) ? a : b;
}

#pragma region CIRCLE_QUEUE
CircleQueue::CircleQueue()
	: m_pBuffer(nullptr)
	, m_bufferSize(0)
	, m_readIndex(0)
	, m_writeIndex(0)
	, m_nWrittenBytes(0)
{}

bool CircleQueue::Initialize(void* pBuffer, SIZE_T bufferSize)
{
	m_pBuffer = (BYTE*)pBuffer;
	m_bufferSize = bufferSize;
	m_readIndex = 0;
	m_writeIndex = 0;

	return true;
}

SIZE_T CircleQueue::GetWritableSize()
{
	// m_readIndex == m_writeIndex일 때
	// 즉 우연히 Queue가 정확히 꽉 찼을 때 
	// ret이 bufferSize로 나와서 나중에 ReadDone() 할 때
	// 에러가 발생하는 버그 수정
	SIZE_T ret = m_bufferSize - m_nWrittenBytes;
	
	// 기존 코드: 
	/*
	if (m_readIndex <= m_writeIndex)
		ret = m_bufferSize - m_writeIndex + m_readIndex;
	else
		ret = m_readIndex - m_writeIndex;
	assert(ret == m_bufferSize - m_nWrittenBytes);
	*/
	
	return ret;
}

void* CircleQueue::WriteRaw(const void* pSource, SIZE_T nBytes)
{
	void* pRet = &m_pBuffer[m_writeIndex];

	if (pSource == nullptr || nBytes == 0)
		return pRet;

	SIZE_T writableSize = GetWritableSize();
	if (nBytes > writableSize)
		return nullptr;

	m_nWrittenBytes += nBytes;
	if (m_readIndex <= m_writeIndex)
	{
		SIZE_T nWrite = Min(m_bufferSize - m_writeIndex, nBytes);
		memcpy(&m_pBuffer[m_writeIndex], pSource, nWrite);

		m_writeIndex = (m_writeIndex + nWrite) % m_bufferSize;

		nBytes -= nWrite;
		if (nBytes == 0)
			return pRet;

		memcpy(m_pBuffer, &((BYTE*)pSource)[nWrite], nBytes);
		m_writeIndex = nBytes;
	}
	else
	{
		memcpy(&m_pBuffer[m_writeIndex], pSource, nBytes);
		m_writeIndex += nBytes;
	}

	return pRet;
}

SIZE_T CircleQueue::GetReadableSize()
{
	SIZE_T ret = m_nWrittenBytes;
	/*
	if (m_readIndex <= m_writeIndex)
		ret = m_writeIndex - m_readIndex;
	else
		ret = m_bufferSize - m_readIndex + m_writeIndex;
	assert(ret == m_nWrittenBytes);
	*/

	return ret;
}

SIZE_T CircleQueue::PeekRaw(WSABUF* pBuf1, WSABUF* pBuf2)
{
	assert(pBuf1 != nullptr);
	assert(pBuf2 != nullptr);

	if (m_readIndex <= m_writeIndex)
	{
		pBuf1->buf = (CHAR*)&m_pBuffer[m_readIndex];
		pBuf1->len = (ULONG)GetReadableSize();
		pBuf2->len = 0;
	}
	else
	{
		pBuf1->buf = (CHAR*)&m_pBuffer[m_readIndex];
		pBuf1->len = (ULONG)(m_bufferSize - m_readIndex);

		pBuf2->buf = (CHAR*)m_pBuffer;
		pBuf2->len = (ULONG)m_writeIndex;
	}

	SIZE_T ret = GetReadableSize();
	assert(ret == pBuf1->len + pBuf2->len);
	// assert(ret == m_nWrittenBytes);
	
	return ret;
}

bool CircleQueue::ReadDone(SIZE_T nBytes)
{
	SIZE_T readableSize = GetReadableSize();
	if (nBytes > readableSize)
		return false;

	m_readIndex = (m_readIndex + nBytes) % m_bufferSize;
	m_nWrittenBytes -= nBytes;
	assert(m_nWrittenBytes >= 0);

	return true;
}
#pragma endregion

#pragma region SEND_BUFFER
SendBuffer::SendBuffer()
	: m_queue()
{}

bool SendBuffer::Initialize(void* pBuffer, SIZE_T bufferSize)
{
	return m_queue.Initialize(pBuffer, bufferSize);
}

bool SendBuffer::Write(IPackable& packable)
{
	SIZE_T sz = packable.ComputePackedSize();
	if (sz >= MAX_PAYLOAD_SIZE)
		assert(0);

	unsigned short payloadSize = (unsigned short)sz;
	bool bRes = m_queue.WriteUShort(payloadSize);
	if (bRes == false)
		return false;

	bool bRet = packable.Pack(&m_queue);

	return bRet;
}

bool SendBuffer::ReadDone(SIZE_T nBytes)
{
	bool bRet = m_queue.ReadDone(nBytes);
	
	return bRet;
}

SIZE_T SendBuffer::Peek(WSABUF* pBuf1, WSABUF* pBuf2)
{
	SIZE_T ret = m_queue.PeekRaw(pBuf1, pBuf2);

	return ret;
}
#pragma endregion
