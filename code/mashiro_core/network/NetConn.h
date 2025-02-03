#pragma once

#include "buffer/MessageQueue.h"
#include "Overlapped.h"

class NetIO;

class NetConn
{
public:
	friend class NetIO;

public:
	enum class eStatus
	{
		Accepting,		// AcceptEx()가 pend된 상태
		Accepted,		// handleAccept()가 발생한 상태
		InUse,			// ID를 통해 접근할 수 있는 상태: 접속이 끊겼을 수 있음

	};

public:
	NetConn();
	void Init(NetConnID id, SOCKET socket, NetIO* pNetIO, AcceptOverlapped* pAccept);

	NetConnID		GetID();
	NetIO*			GetNetIO();

	SOCKADDR_IN6*	GetLocalAddr();
	SOCKADDR_IN6*	GetRemoteAddr();

	int GetPort();
	int GetIPVersion();					// AF_INET or AF_INET6
	std::string_view GetIPString();		// ret: sz

	void*			GetAttachment();
	void*			SetAttachment(void* pAttach);			// return old value

	bool			Send(Packet& packet, bool bNow);
	void			Disconnect();

private:
	eStatus				m_status;
	
	NetConnID			m_id;
	// m_lock을 걸고 확인해야 한다. INVALID_SOCKET이면 disconnect 된 것을 의미한다. 
	SOCKET				m_socket;
	NetIO*				m_pNetIO;

	SRWLOCK				m_lock;			// for for m_refCount & m_socket
	int					m_refCount;		// default: 1(initial recv preempted)
	
	// AcceptOverlapped*	m_pAccept;		// NetConnJob을 통해 통지된 후 nullptr이 된다. 
	SendOverlapped		m_send;
	RecvOverlapped		m_recv;

	BYTE				m_addrBuffer[ADDR_SIZE * 2];
	SOCKADDR*			m_pLocalAddr;		// m_addrBuffer를 가르킨다. 
	SOCKADDR*			m_pRemoteAddr;		// m_addrBuffer를 가르킨다.

	void*				m_pAttachment;

};
