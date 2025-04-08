#pragma once

#include "Message.h"
#include "buffer/ReservingBuffer.h"

class IJobQueue
{
public:
	virtual void CleanUp() = 0;

	virtual bool PushJob(int workerIndex, Job& job) = 0;
	virtual bool TryPushJob(int workerIndex, Job& job) = 0;

	virtual bool IsReadWriteEmpty() = 0;

};

class MessageQueue final : public IJobQueue
{
public:
	struct Popped
	{
		Popped();

		eMessageType	messageType;
		PackableType	packableType;

		SenderID		senderID;		// if not packet, INVALID_CONNECTION
		RangedReader	messageReader;

	};

public:
	MessageQueue();
	MessageQueue(MessageQueue& other) = delete;
	MessageQueue(MessageQueue&& other) = delete;
	virtual ~MessageQueue();

	bool Init(DWORD popThreadID, SIZE_T bufferSize);	// popThreadID: GetCurrentThreadId()
	bool IsNull();
	void CleanUp() override;

	void SetPopThreadID(DWORD threadID);				// set after Init()

public:
	// Push() Functions
	// in case of insufficient memory: 
	// - PopThread: return false
	// - OtherThread: sleep & rety untill success

	bool PushEvent(Event& event);
	bool PushJob(Job& job);
	bool PushPacket(NetConnID senderID, Packet& packet);
	bool PushPacket(NetConnID senderID, void* pPackedPacket, SIZE_T packetSize);

	bool TryPushEvent(Event& event);
	bool TryPushJob(Job& job);

public:		// IJobQueue implementation: ignore workerIndex
	bool PushJob(int workerIndex, Job& job) override;
	bool TryPushJob(int workerIndex, Job& job) override;

public:		// must be executed on popThread only
	bool Pop(Popped* pPopped);

	// must be called after Pop() returned false, or assert
	// if nothing to read, no swap and return false
	bool SwapForPopMore();

	HANDLE GetPushEvent();
	HANDLE GetSwapEvent();

	bool IsReadWriteEmpty() override;

private:
	DWORD				m_popThreadID;

	SRWLOCK				m_swapLock;
	ReservingBuffer		m_buffers[2];
	ReservingBuffer*	m_pReadBuffer;
	ReservingBuffer*	m_pWriteBuffer;

	HANDLE				m_pushEvent;		// MaunalReset, SetEvent() on Push...(), ResetEvent() on Pop()
	HANDLE				m_swapEvent;		// MaunalReset, PulseEvent() on Swap()

};
using MsgQueue = MessageQueue;
