#pragma once

#include "buffer/Pool.h"
#include "NetConn.h"

constexpr int MAX_CONNECTION = 8192;
constexpr int MAX_ACCEPT = 256;
constexpr int SEND_THRESHOLD_SIZE = 512;
constexpr float SEND_THRESHOLD_TIME = 1000.0f;		// ms

using OnNetConn = bool (*)(NetConn* pConn, bool isNet);

enum class eSendResult
{
	None = 0,				// default value before function call
	Disconnected,			// 실패: send()를 호출하기 전에 이미 disconnect()됨
	Overflow,				// 실패: sendBuffer가 넘쳐 disconnect()됨
	AlreadySending,			// 성공: 이미 WSASend()가 진행중임
	NothingToSend,			// 성공: Send 할 내용이 없음
	UnderThreshold,			// 성공: WSASend()를 pend할 조건이 충족되지 않음
	PendFailed,				// 실패: WSASend() 에러
	SendPended,				// 성공: WSASend() 발생

};

class NetIO
{
public:
	friend class NetConnJob;

public:
	static UINT worker(void* pParam);

public:
	NetIO();
	bool		Init(MsgQueue* pQueue, OnNetConn callback);
	void		StartListen(int port, int workerCount);
	void		StopListen();						// close listen socket(m_mainSocket)
	void		CleanUp();							// stop worker and release everything

	bool		PendAccept();						// 호출 해놓지 않으면 새로운 NetConn이 연결되지 않는다. 
	void		FlushSend(LARGE_INTEGER now);		// 일정 주기마다 호출해 NetConn의 SendBuffer를 비운다. 

	// 이미 disconnect됐지만 아직 callback을 통해 통지되지 않은 경우, nullptr을 리턴하지 않는다. 
	NetConn*	GetConn(NetConnID id);
	bool		_Send(NetConn* pConn, Packet& packet, bool bNow);		// deprecated
	void		Disconnect(NetConn* pConn);
	void		DisconnectAll();

public:		// Send2
	// 기존 _Send()와 달리 bUrgent여도 WSASend를 발생시키지 않고, 대신 m_urgentList에 Append한다. 
	bool		Send(NetConn* pConn, Packet& packet, bool bUrgent);
	void		FlushUrgentSend();				// m_urgentList를 비운다. 

protected:	// called by worker
	void handleAccept(AcceptOverlapped* pAccept, DWORD dwError);
	void handleRecv(NetConn* pConn, RecvOverlapped* pRecv, DWORD nBytes);	// nbytes == 0 일 수 있다. 
	void handleSend(NetConn* pConn, SendOverlapped* pSend, DWORD nBytes);	// nbytes == 0 일 수 있다. 

protected:
	SOCKET			createNonblockingSocket(ADDRINFO* pAddrinfo, bool bNagle);
	bool			pendRecv(NetConn* pConn);
	bool			pushCompletedPackets(NetConn* pConn, DWORD nbytes);					// false를 리턴한 경우 disconnect 시킨다. 
	eSendResult		send(NetConn* pConn, Packet* pPacket, bool bFlush);					// pPacket: nullable
	bool			pendSend(LARGE_INTEGER now, NetConn* pConn, WSABUF buffers[2]);		// send() 함수에서 사용
	void			disconnect(NetConn* pConn);											// IO 실패 시 호출한다.
	void			pushDeletConnJob(NetConn* pConn);									// NetConn::m_refCount == 0이 됐을 때 한 번 호출한다. 
	bool			executeNetConnJob(NetConn* pConn, bool isNew);						// NetConnJob::Execute()

protected:
	MessageQueue*		m_pQueue;
	OnNetConn			m_callback;

	int					m_port;
	ADDRINFO*			m_pAddrinfo;
	SOCKET				m_mainSocket;
	bool				m_isListenSocket;

	int					m_workerCount;
	HANDLE				m_iocp;
	HANDLE*				m_threads;

	SRWLOCK				m_acceptListLock;
	LinkedList			m_acceptList;
	AcceptOverlapped	m_accepts[MAX_ACCEPT];

	int					m_idCount;
	NetConnID			m_lastID;
	NetConn*			m_pConns[MAX_CONNECTION];

	SRWLOCK				m_connPoolLock;
	Pool				m_connPool;		// PendAccept() 시 할당

	SRWLOCK				m_flushListLock;
	LinkedList			m_flushList;

	SRWLOCK				m_urgentListLock;
	LinkedList			m_urgentList;

};

// queue를 통해 메인 스레드에게 new NetConn/Disconn을 알린다. 
class NetConnJob : public Job
{
public:
	NetConnJob();

	bool Execute();

protected:
	bool packField(IWriter* pWriter) override;
	bool unpackField(IReader* pReader) override;

public:
	NetConn*	pConn;
	bool		isNew;

};
