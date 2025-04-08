#include "network/NetIO.h"
#include "network/NetConn.h"

using namespace overlapped;

NetConn::NetConn()
	: m_status(eStatus::Accepted)
	, m_id(INVALID_CONNECTION)
	, m_socket(INVALID_CONNECTION)
	, m_pNetIO(nullptr)
	, m_lock{}
	, m_refCount(0)
	, m_send{ 0 }
	, m_recv{ 0 }
	, m_addrBuffer{ 0 }
	, m_pLocalAddr(nullptr)
	, m_pRemoteAddr(nullptr)
	, m_pAttachment(nullptr)
{}

void NetConn::Init(NetConnID id, SOCKET socket, NetIO* pNetIO, AcceptOverlapped* pAccept)
{
	assert(id != INVALID_CONNECTION);
	assert(socket != INVALID_SOCKET);
	assert(pNetIO != nullptr);

	m_status = eStatus::Accepting;

	m_id = id;
	m_socket = socket;
	m_pNetIO = pNetIO;

	InitializeSRWLock(&m_lock);
	m_refCount = 1;		// initial recv

	// m_pAccept = pAccept;

	m_send.op = eOperation::Send;
	InitializeSRWLock(&m_send.lock);
	m_send.sendBuffer.Initialize(m_send.buffer, SEND_BUFFER_SIZE);
	m_send.lastSendTime = QCGetCounter();
	m_send.isSending = false;
	m_send.bFlush = false;
	m_send.bToBeFlushed = false;
	m_send.flushThreshold = LARGE_INTEGER{};
	m_send.flushLink.pItem = this;
	m_send.flushLink.pPrev = nullptr;
	m_send.flushLink.pNext = nullptr;

	m_send.bToBeUrgentFlushed = false;
	m_send.urgentLink.pItem = this;
	m_send.urgentLink.pPrev = nullptr;
	m_send.urgentLink.pNext = nullptr;

	m_recv.op = eOperation::Recv;
	m_recv.readIndex = 0;
	m_recv.writeIndex = 0;

	m_pLocalAddr = nullptr;
	m_pRemoteAddr = nullptr;

	m_pAttachment = nullptr;
}

NetConnID NetConn::GetID()
{
	return m_id;
}

NetIO* NetConn::GetNetIO()
{
	return m_pNetIO;
}

SOCKADDR_IN6* NetConn::GetLocalAddr()
{
	return (SOCKADDR_IN6*)m_pLocalAddr;
}

SOCKADDR_IN6* NetConn::GetRemoteAddr()
{
	return (SOCKADDR_IN6*)m_pRemoteAddr;
}

int NetConn::GetPort()
{
	SOCKADDR_IN6* pRemoteAddr = GetRemoteAddr();
	int ret = pRemoteAddr->sin6_port;

	return ret;
}

int NetConn::GetIPVersion()
{
	SOCKADDR_IN6* pRemoteAddr = GetRemoteAddr();
	IN6_ADDR* pInAddr6 = &pRemoteAddr->sin6_addr;
	bool isV4 = IN6_IS_ADDR_V4MAPPED(pInAddr6);
	if (isV4)
		return AF_INET;

	return AF_INET6;
}

std::string_view NetConn::GetIPString()
{
	static char ip_str_buffer[INET6_ADDRSTRLEN] = { 0 };

	SOCKADDR_IN6* pRemoteAddr = GetRemoteAddr();
	int ipVersion = GetIPVersion();

	void* pAddr = &pRemoteAddr->sin6_addr;
	if (ipVersion == AF_INET)
	{
		IN_ADDR* pInAddr = (IN_ADDR*)&pRemoteAddr->sin6_addr;
		pAddr = &pInAddr[3];		// skip 0:0:0:0:0:FFFF
	}
	CHAR* pRes = (CHAR*)inet_ntop(ipVersion, pAddr, ip_str_buffer, INET6_ADDRSTRLEN);
	assert(pRes == ip_str_buffer);
	int len = strnlen_s(ip_str_buffer, INET6_ADDRSTRLEN);

	std::string_view ret(ip_str_buffer, len);

	return ret;
}

void* NetConn::GetAttachment()
{
	return m_pAttachment;
}

void* NetConn::SetAttachment(void* pAttach)
{
	void* pRet = m_pAttachment;
	m_pAttachment = pAttach;

	return pRet;
}

bool NetConn::Send(Packet& packet, bool bNow)
{
	assert(m_pNetIO != nullptr);

	bool bRet = m_pNetIO->Send(this, packet, bNow);

	return bRet;
}

void NetConn::Disconnect()
{
	assert(m_pNetIO != nullptr);

	m_pNetIO->Disconnect(this);
}