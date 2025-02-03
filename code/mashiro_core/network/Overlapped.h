#pragma once

#include "Packable.h"
#include "buffer/LinkedList.h"

constexpr int ADDR_SIZE = (sizeof(sockaddr_storage) + 16);
constexpr int MAX_PAYLOAD_SIZE = 1024;
// constexpr int SEND_BUFFER_SIZE = 1024 * 16;
constexpr int SEND_BUFFER_SIZE = 1024 * 256;	// TEST
constexpr int RECV_BUFFER_SIZE = 1024 * 16;

class NetConn;

namespace overlapped
{
	enum class eOperation : BYTE
	{
		Accept = 0,
		Send,
		Recv,
		Stop,

	};

	struct OverlappedEx : public WSAOVERLAPPED
	{
		eOperation op;

	};

	struct AcceptOverlapped : public OverlappedEx
	{
		NetConn* pConn;
		DWORD	bytesReceived;

		Link	link;

	};

	SIZE_T Min(SIZE_T a, SIZE_T b);
	SIZE_T Max(SIZE_T a, SIZE_T b);

	class CircleQueue : public IWriter
	{
	public:
		CircleQueue();

		bool			Initialize(void* pBuffer, SIZE_T bufferSize);

		virtual SIZE_T	GetWritableSize() override;
		virtual void*	WriteRaw(const void* pSource, SIZE_T nBytes);

		SIZE_T			GetReadableSize();
		SIZE_T			PeekRaw(WSABUF* pBuf1, WSABUF* pBuf2);
		bool			ReadDone(SIZE_T nBytes);

	public:
		BYTE*	m_pBuffer;
		SIZE_T	m_bufferSize;

		SIZE_T	m_readIndex;
		SIZE_T	m_writeIndex;

		SIZE_T	m_nWrittenBytes;

	};

	class SendBuffer
	{
	public:
		SendBuffer();

		bool	Initialize(void* pBuffer, SIZE_T bufferSize);

		bool	Write(IPackable& packable);				// send

		bool	ReadDone(SIZE_T nBytes);				// handleRecv
		SIZE_T	Peek(WSABUF* pBuf1, WSABUF* pBuf2);		// pendRecv

	private:
		CircleQueue m_queue;

	};

	struct RecvOverlapped : public OverlappedEx
	{
		CHAR		buffer[RECV_BUFFER_SIZE];

		int			readIndex;
		int			writeIndex;

	};

	struct SendOverlapped : public OverlappedEx
	{
		SRWLOCK			lock;

		BYTE			buffer[SEND_BUFFER_SIZE];
		SendBuffer		sendBuffer;

		bool			isSending;
		bool			bFlush;
		
		bool			bToBeFlushed;
		LARGE_INTEGER	flushThreshold;		// threshold 가 지나야 flush한다. 
		Link			flushLink;			// m_flushList에서 사용되는 link

		bool			bToBeUrgentFlushed;
		Link			urgentLink;

		LARGE_INTEGER	lastSendTime;

	};

}
using AcceptOverlapped	= overlapped::AcceptOverlapped;
using RecvOverlapped	= overlapped::RecvOverlapped;
using SendOverlapped	= overlapped::SendOverlapped;
