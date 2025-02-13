#include "network/NetIO.h"

using namespace overlapped;

LPFN_ACCEPTEX lpfnAcceptEx = nullptr;
LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs = nullptr;

#pragma region NET_IO
unsigned int NetIO::worker(void* pParam)
{
	NetIO* pNetIO = (NetIO*)pParam;

	printf("[Info]NetIO::worker() started. Thread ID: %d \n", GetCurrentThreadId());
	defer{
		printf("[Info]NetIO::worker() stopped. Thread ID: %d \n", GetCurrentThreadId());
	};

	while (true)
	{
		DWORD nBytes = 0;
		NetConn* pConn = nullptr;	// completion key
		OverlappedEx* pOverlapped = nullptr;
		bool bRes = GetQueuedCompletionStatus(pNetIO->m_iocp, &nBytes, (PULONG_PTR)&pConn, (OVERLAPPED**)&pOverlapped, INFINITE);
		DWORD dwError = NO_ERROR;
		if (bRes == false)
			dwError = GetLastError();

		eOperation op = eOperation::Stop;
		if (pOverlapped != nullptr)
			op = pOverlapped->op;

		if ((bRes == false) || (pOverlapped == nullptr))
		{
			switch (dwError)
			{
				// known errors
			case ERROR_NETNAME_DELETED:		// 서버 입장에서 클라가 강제 종료함
			case ERROR_CONNECTION_ABORTED:	// 클라 입장에서 서버에서 shutdown() or closesocket() 발생
			case WSA_OPERATION_ABORTED:		// pend IO 있는 상태에서 closesocket
			{
				assert(pOverlapped != nullptr);

				if (pOverlapped->op == eOperation::Send)
				{
					assert(pConn != nullptr);
					SendOverlapped* pSend = (SendOverlapped*)pOverlapped;
					pNetIO->handleSend(pConn, pSend, 0);
				}
				else if (pOverlapped->op == eOperation::Recv)
				{
					assert(pConn != nullptr);
					RecvOverlapped* pRecv = (RecvOverlapped*)pOverlapped;
					pNetIO->handleRecv(pConn, pRecv, 0);
				}
				else if (pOverlapped->op == eOperation::Accept)
				{
					assert(pConn == nullptr);
					assert(
						dwError == ERROR_NETNAME_DELETED ||			// The client disconnected immediately after successfully connecting.
						dwError == ERROR_OPERATION_ABORTED			// listen socket is closed
					);

					// Accept 발생해서 OK 하려고 했는데 그 새 또 삭제됨
					AcceptOverlapped* pAccept = (AcceptOverlapped*)pOverlapped;
					pNetIO->handleAccept(pAccept, dwError);
				}
				
				break;		// continue loop
			}
			case ERROR_SEM_TIMEOUT:			// 노트북으로 2048명 접속시키니까 Recv에서 뜬다.. 
			{
				// 검색해보니.. 
				// REF: https://darozzang.tistory.com/entry/Connection-Error-IOCP-GetQueuedCompletionStatus
				// 장비(방화벽 또는 라우터) 이상 또는 제한으로 인한 
				// 네트워크 단절(랜선을 뽑는 경우도 포함)이 발생했을 때
				// 뜬다고 한다. 

				assert(pOverlapped != nullptr);
				if (pOverlapped->op == eOperation::Recv)
				{
					printf("[error]Recv ERROR_SEM_TIMEOUT \n");

					assert(pConn != nullptr);
					RecvOverlapped* pRecv = (RecvOverlapped*)pOverlapped;
					pNetIO->handleRecv(pConn, pRecv, 0);
				}
				else
					assert(0);

				break;
			}
			// what? 
			case WAIT_TIMEOUT:
			default:
			{
				//if ((pOverlapped != nullptr) && (op == eOperation::Accept))	// accept failed?
				//{
				//	LOG_ERROR_STOP("GetQueuedCompletionStatus() pended WSAAccept() Failed. dwError: %d", dwError);

				//	break;		// continue loop
				//}

				// 더미 클라이언트 2048개 접속시키니까
				// Recv에서 ERROR_SEM_TIMEOUT 121(0x79) 뜬다.. 


				// 뭘까?
				LOG_ERROR_STOP("GetQueuedCompletionStatus() error. \n - dwError: %d", dwError);

				break;		// continue loop
			}
			}		// switch (dwError)

			continue;		// continue loop anyway
		}

		switch (op)
		{
		case overlapped::eOperation::Accept:
		{
			AcceptOverlapped* pAccept = (AcceptOverlapped*)pOverlapped;
			pNetIO->handleAccept(pAccept, dwError);

			continue;		// continue loop
		}
		case overlapped::eOperation::Send:
		{
			SendOverlapped* pSend = (SendOverlapped*)pOverlapped;
			pNetIO->handleSend(pConn, pSend, nBytes);

			continue;		// continue loop
		}
		case overlapped::eOperation::Recv:
		{
			RecvOverlapped* pRecv = (RecvOverlapped*)pOverlapped;
			pNetIO->handleRecv(pConn, pRecv, nBytes);

			continue;		// continue loop
		}
		case overlapped::eOperation::Stop:		// stop thread
		{
			return 0;
		}
		default:
		{
			assert(0);

			return 1;
		}
		}		// end of switch (op)
	}		// end of while(true)

	return 0;
}

NetIO::NetIO()
	: m_pQueue(nullptr)
	, m_callback(nullptr)
	, m_port(0)
	, m_pAddrinfo(nullptr)
	, m_mainSocket(INVALID_SOCKET)
	, m_isListenSocket(false)
	, m_workerCount(0)
	, m_iocp(INVALID_HANDLE_VALUE)
	, m_threads(nullptr)
	, m_acceptListLock{}
	, m_acceptList()
	, m_accepts{}
	, m_idCount(0)
	, m_lastID(INVALID_CONNECTION)
	, m_pConns{}
	, m_connPoolLock{}
	, m_connPool()
	, m_flushListLock{}
	, m_flushList()
	, m_urgentListLock{}
	, m_urgentList()
{}

bool NetIO::Init(MsgQueue* pQueue, OnNetConn callback)
{
	assert(m_pQueue == nullptr);

	m_pQueue = pQueue;
	m_callback = callback;

	m_port = 0;
	m_pAddrinfo = nullptr;
	m_mainSocket = INVALID_SOCKET;
	m_isListenSocket = false;

	m_workerCount = 0;
	m_iocp = INVALID_HANDLE_VALUE;
	m_threads = nullptr;

	InitializeSRWLock(&m_acceptListLock);
	m_acceptList.count = 0;
	m_acceptList.pHead = 0;
	m_acceptList.pLast = 0;
	for (int i = 0; i < MAX_ACCEPT; i++)
	{
		AcceptOverlapped& iAccept = m_accepts[i];
		iAccept.op = eOperation::Accept;
		iAccept.pConn = nullptr;
		iAccept.link.pPrev = nullptr;
		iAccept.link.pNext = nullptr;
		iAccept.link.pItem = &iAccept;
		AppendToList(&m_acceptList, &iAccept.link);
	}

	m_idCount = 0;
	m_lastID = INVALID_CONNECTION;
	ZeroMemory(&m_pConns, MAX_CONNECTION * sizeof(NetConn*));

	InitializeSRWLock(&m_connPoolLock);
	// m_connPool.Init(sizeof(NetConn), 32);
	m_connPool.Init(sizeof(NetConn), 512);

	InitializeSRWLock(&m_flushListLock);
	m_flushList.count = 0;
	m_flushList.pHead = 0;
	m_flushList.pLast = 0;

	InitializeSRWLock(&m_urgentListLock);
	m_urgentList.count = 0;
	m_urgentList.pHead = 0;
	m_urgentList.pLast = 0;

	return true;
}

void NetIO::StartListen(int port, int workerCount)
{
	m_port = port;
	char port_c_str[16] = { 0 };
	_itoa_s(port, port_c_str, 10);

	addrinfo hints = { 0 };
	hints.ai_family = AF_INET6;		// AF_UNSPEC, AF_INET, AF_INET6
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_socktype = (hints.ai_protocol == IPPROTO_TCP) ? SOCK_STREAM : SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	int iRes = getaddrinfo(nullptr, port_c_str, &hints, &m_pAddrinfo);
	if (iRes != NO_ERROR)
		LOG_ERROR_STOP("getaddrinfo() failed. LastError: %d", WSAGetLastError());
	if (m_pAddrinfo == nullptr)
		LOG_ERROR_STOP("getaddrinfo() result is NULL. ");

	m_mainSocket = createNonblockingSocket(m_pAddrinfo, false);
	if (m_mainSocket == INVALID_SOCKET)
		LOG_ERROR_STOP("createNonblockingSocket() failed. ");

	if (lpfnAcceptEx == nullptr)	// load AcceptEx if nullptr
	{
		GUID guidAcceptEx = WSAID_ACCEPTEX;
		DWORD dwBytes;
		int iRes = WSAIoctl(
			m_mainSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guidAcceptEx, sizeof(guidAcceptEx),
			&lpfnAcceptEx, sizeof(lpfnAcceptEx),
			&dwBytes, nullptr, nullptr
		);
		if (iRes == SOCKET_ERROR)
			LOG_ERROR_STOP("WSAIoctl() failed. LastError: %d ", WSAGetLastError());
	}

	if (lpfnGetAcceptExSockaddrs == nullptr)
	{
		DWORD dwBytes = 0;
		GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
		int iRes = WSAIoctl(m_mainSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guidGetAcceptExSockaddrs, sizeof(guidGetAcceptExSockaddrs),
			&lpfnGetAcceptExSockaddrs, sizeof(lpfnGetAcceptExSockaddrs),
			&dwBytes, nullptr, nullptr
		);
		if (iRes == SOCKET_ERROR)
			LOG_ERROR_STOP("WSAIoctl failed: %d \n", WSAGetLastError());
	}

	iRes = bind(m_mainSocket, m_pAddrinfo->ai_addr, (int)m_pAddrinfo->ai_addrlen);
	if (iRes != NO_ERROR)
		LOG_ERROR_STOP("bind() failed. LastError: %d", WSAGetLastError());

	int backlog = SOMAXCONN;	// max length of pending connection queue. FD_SETSIZE?
	iRes = listen(m_mainSocket, backlog);
	if (iRes != NO_ERROR)
		LOG_ERROR_STOP("listen() failed. LastError: %d", WSAGetLastError());
	m_isListenSocket = true;

	m_workerCount = workerCount;
	int concurrentWorkerCount = m_workerCount;
	m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrentWorkerCount);
	if (m_iocp == INVALID_HANDLE_VALUE)
		LOG_ERROR_STOP("CreateIoCompletionPort() failed. LastError: %d", GetLastError());

	// associate listen socket
	HANDLE hRes = CreateIoCompletionPort((HANDLE)m_mainSocket, m_iocp, 0, 0);
	if (hRes != m_iocp)
		LOG_ERROR_STOP("CreateIoCompletionPort() failed. LastError: %d ", GetLastError());

	// start threads
	m_threads = new HANDLE[workerCount];
	for (int i = 0; i < workerCount; i++)
	{
		m_threads[i] = (HANDLE)_beginthreadex(nullptr, 0, worker, this, 0, nullptr);
		if (m_threads[i] == INVALID_HANDLE_VALUE || m_threads[i] == 0)
			LOG_ERROR_STOP("_beginthreadex() failed. errno: %d ", GetLastError());
	}
}

void NetIO::StopListen()
{
	if (m_mainSocket == INVALID_SOCKET)
		DebugBreak();

	shutdown(m_mainSocket, SD_BOTH);
	closesocket(m_mainSocket);
	m_mainSocket = INVALID_SOCKET;

	DisconnectAll();
}

void NetIO::CleanUp()
{
	m_pQueue = nullptr;
	m_callback = nullptr;
	m_port = 0;

	if (m_pAddrinfo != nullptr)
	{
		freeaddrinfo(m_pAddrinfo);
		m_pAddrinfo = nullptr;
	}

	if (m_mainSocket != INVALID_SOCKET)
		DebugBreak();
	m_isListenSocket = false;

	// stop all threads
	OverlappedEx stopOverlapped;
	stopOverlapped.op = eOperation::Stop;
	for (int i = 0; i < m_workerCount; i++)
	{
		bool bRes = PostQueuedCompletionStatus(m_iocp, 0, (ULONG_PTR)nullptr, &stopOverlapped);
		if (bRes == false)
			DebugBreak();
	}
	DWORD dwObject = WaitForMultipleObjects(m_workerCount, m_threads, true, INFINITE);
	if (dwObject == WAIT_FAILED)
		DebugBreak();
	if (dwObject == WAIT_TIMEOUT)
		DebugBreak();

	CloseHandle(m_iocp);
	m_iocp = INVALID_HANDLE_VALUE;
	for (int i = 0; i < m_workerCount; i++)
		CloseHandle(m_threads[i]);
	delete m_threads;
	m_threads = nullptr;
	m_workerCount = 0;

	// every pended WSAAccept() should be already canceled 
	if (m_acceptList.count != MAX_ACCEPT)
		DebugBreak();
	Link* pLink = m_acceptList.pHead;
	while (pLink != nullptr)
	{
		AcceptOverlapped* pAccept = (AcceptOverlapped*)pLink->pItem;
		Link* pNextLink = pLink->pNext;
		
		if (pAccept->pConn != nullptr)
			DebugBreak();

		pLink = pNextLink;
	}
	
	m_idCount = 0;
	m_lastID = INVALID_CONNECTION;

	if (m_connPool.GetCreatedEntryCount() != 0)
		DebugBreak();
	m_connPool.CleanUp();

	if (m_flushList.count != 0)
		DebugBreak();

	if (m_urgentList.count != 0)
		DebugBreak();
}

bool NetIO::PendAccept()
{
	if (m_idCount >= MAX_CONNECTION)		// 더 이상 NetConnID를 발급할 수 없음
		return false;

	// get AcceptOverlapped
	AcceptOverlapped* pAccept = nullptr;
	{
		LOCK_EXCLUSIVE(m_acceptListLock);

		Link* pLink = PopFirstFromList(&m_acceptList);
		if (pLink == nullptr)
			return false;

		pAccept = (AcceptOverlapped*)pLink->pItem;
		assert(pAccept != nullptr);
	}

	if (pAccept->pConn != nullptr)
		DebugBreak();

	// TODO: 발급기 사용
	// new NetConnID
	while (true)
	{
		m_lastID++;
		bool isValid = (m_pConns[m_lastID % MAX_CONNECTION] == nullptr) && (m_lastID != INVALID_CONNECTION);
		if (isValid)		// ret is ret
			break;
	}
	// m_lastID is now new valid NetConnID

	// new NetConn
	NetConn* pConn = nullptr;
	{
		LOCK_EXCLUSIVE(m_connPoolLock);

		pConn = (NetConn*)m_connPool.Create();
		pConn = new(pConn)NetConn();
		// pConn = new NetConn();
	}

	// socket for new NetConn
	SOCKET socket = createNonblockingSocket(m_pAddrinfo, true);
	HANDLE hRes = CreateIoCompletionPort((HANDLE)socket, m_iocp, (ULONG_PTR)pConn, 0);
	if (hRes == NULL)
		LOG_ERROR_STOP("CreateIoCompletionPort failed.LastError: %d \n", WSAGetLastError());

	pConn->Init(m_lastID, socket, this, pAccept);

	// register it
	m_pConns[m_lastID % MAX_CONNECTION] = pConn;

	// finally, 
	pAccept->pConn = pConn;
	bool bRes = lpfnAcceptEx(
		m_mainSocket, pConn->m_socket,
		pConn->m_addrBuffer, 0,
		ADDR_SIZE, ADDR_SIZE,
		&pAccept->bytesReceived,
		pAccept
	);
	if (bRes == false)		// must success
	{
		int iError = WSAGetLastError();
		if (iError != WSA_IO_PENDING)
			LOG_ERROR_STOP("AcceptEx() failed. LastError: %d", iError);
	}

	return true;
}

void NetIO::FlushSend(LARGE_INTEGER now)
{
	while (true)
	{
		NetConn* pConn = nullptr;
		Link* pLink = nullptr;
		{
			LOCK_EXCLUSIVE(m_flushListLock);
			{
				if (m_flushList.count == 0)
					return;

				pConn = (NetConn*)m_flushList.pHead->pItem;
				if (pConn == nullptr)		// 이미 접속이 끊김
				{
					Link* pLink = PopFirstFromList(&m_flushList);
					assert(pLink != nullptr);

					continue;
				}

				SendOverlapped& sendOverlapped = pConn->m_send;
				float elapsed = QCMeasureElapsedTick(now, sendOverlapped.flushThreshold);
				if (elapsed < 0.0f)		// 아직 시간이 지나지 않음
					return;

				Link* pLink = PopFirstFromList(&m_flushList);
				assert(pLink != nullptr);
			}
		}

		{
			LOCK_EXCLUSIVE(pConn->m_send.lock);
			assert(pConn->m_send.bToBeFlushed == true);
			send(pConn, nullptr, true);
			pConn->m_send.bToBeFlushed = false;
		}
	}
}

NetConn* NetIO::GetConn(NetConnID id)
{
	NetConn* pRet = m_pConns[id % MAX_CONNECTION];
	if (pRet == nullptr)
		return nullptr;
	if (pRet->m_status != NetConn::eStatus::InUse)
		return nullptr;
	if (pRet->m_id != id)
		return nullptr;

	return pRet;
}

bool NetIO::_Send(NetConn* pConn, Packet& packet, bool bNow)
{
	assert(pConn != nullptr);

	SendOverlapped* pSend = &pConn->m_send;
	eSendResult eRes = eSendResult::None; 
	bool isInFlushList = false;
	{
		LOCK_EXCLUSIVE(pSend->lock);
		eRes = send(pConn, &packet, bNow);
		isInFlushList = pSend->bToBeFlushed;
		if (eRes == eSendResult::UnderThreshold)
			pSend->bToBeFlushed = true;
	}
	assert(eRes != eSendResult::None);
	if (eRes == eSendResult::UnderThreshold)
	{
		// flush 처리 할 수 있도록 link에 추가한다. 
		if (isInFlushList == false)
		{
			SendOverlapped& sendOverlapped = pConn->m_send;
			LARGE_INTEGER now = QCGetCounter();
			LOCK_EXCLUSIVE(m_flushListLock);
			sendOverlapped.flushThreshold = QCCounterAddTick(now, SEND_THRESHOLD_TIME);
			AppendToList(&m_flushList, &sendOverlapped.flushLink);
		}
	}
	
	bool bRes = true;
	if (
		(eRes == eSendResult::Disconnected) ||
		(eRes == eSendResult::Overflow) ||
		(eRes == eSendResult::PendFailed)
	)
		bRes = false;

	return bRes;
}

void NetIO::Disconnect(NetConn* pConn)
{
	assert(pConn != nullptr);

	LOCK_EXCLUSIVE(pConn->m_lock);
	disconnect(pConn);
}

void NetIO::DisconnectAll()
{
	for (int i = 0; i < MAX_CONNECTION; i++)
	{
		NetConn* pConn = m_pConns[i];
		if (pConn == nullptr)
			continue;

		Disconnect(pConn);
	}
}

bool NetIO::Send(NetConn* pConn, Packet& packet, bool bUrgent)
{
	assert(pConn != nullptr);


	{
		LOCK_EXCLUSIVE(pConn->m_lock);
		if (pConn->m_socket == INVALID_SOCKET)		// 이미 disconnect됨
			return false;
	}

	bool bAppendUrgent = false;
	bool bAppendFlush = false;
	SendOverlapped& send = pConn->m_send;
	{
		LOCK_EXCLUSIVE(send.lock);

		// sendBuffer에 write
		SendBuffer& sendBuffer = send.sendBuffer;
		bool bWriteRes = sendBuffer.Write(packet);
		if (bWriteRes == false)		// sendBuffer가 꽉 찬 경우
		{
			printf("[info]sendBuffer full: %d \n", pConn->GetID());
			LOCK_EXCLUSIVE(pConn->m_lock);
			disconnect(pConn);

			return false;
		}

		if (send.bToBeUrgentFlushed)
			return true;

		if (bUrgent)
		{
			send.bToBeUrgentFlushed = true;
			bAppendUrgent = true;
		}
		else if (send.bToBeFlushed == false)
		{
			send.bToBeFlushed = true;
			LARGE_INTEGER now = QCGetCounter();
			send.flushThreshold = QCCounterAddTick(now, SEND_THRESHOLD_TIME);
			bAppendFlush = true;
		}
	}

	if (bAppendUrgent)
	{
		LOCK_EXCLUSIVE(m_urgentListLock);
		AppendToList(&m_urgentList, &send.urgentLink);
	}
	else if (bAppendFlush)
	{
		LOCK_EXCLUSIVE(m_flushListLock);
		AppendToList(&m_flushList, &send.flushLink);
	}

	return true;
}

void NetIO::FlushUrgentSend()
{
	LOCK_EXCLUSIVE(m_urgentListLock);
	Link* pLink = m_urgentList.pHead;
	while (pLink != nullptr)
	{
		Link* pNextLink = pLink->pNext;
		NetConn* pConn = (NetConn*)pLink->pItem;
		{
			LOCK_EXCLUSIVE(pConn->m_send.lock);
			send(pConn, nullptr, true);
			pConn->m_send.bToBeUrgentFlushed = false;
		}

		UnlinkFromList(&m_urgentList, &pConn->m_send.urgentLink);

		pLink = pNextLink;
	}
}

void NetIO::handleAccept(AcceptOverlapped* pAccept, DWORD dwError)
{
	assert(pAccept != nullptr);

	NetConn* pConn = pAccept->pConn;
	assert(pConn->m_status == NetConn::eStatus::Accepting);
	
	// The client disconnected immediately after successfully connecting.
	if (dwError == ERROR_NETNAME_DELETED)
	{
		// pend accept again reusing pAccept
		lpfnAcceptEx(
			m_mainSocket, pConn->m_socket,
			pConn->m_addrBuffer, 0,
			ADDR_SIZE, ADDR_SIZE,
			&pAccept->bytesReceived,
			pAccept
		);

		return;
	}

	// return to m_acceptList
	{
		LOCK_EXCLUSIVE(m_acceptListLock);
		AppendToList(&m_acceptList, &pAccept->link);
	}

	pAccept->pConn = nullptr;

	if (dwError != NO_ERROR)
	{
		// what else? 
		assert(dwError == ERROR_OPERATION_ABORTED);

		SOCKET socket = INVALID_SOCKET;
		{
			LOCK_EXCLUSIVE(pConn->m_lock);
			socket = pConn->m_socket;
			pConn->m_socket = INVALID_SOCKET;
		}

		if (socket != INVALID_SOCKET)
		{
			shutdown(socket, SD_BOTH);
			closesocket(socket);
		}

		{
			LOCK_EXCLUSIVE(m_connPoolLock);
			m_connPool.Free(pConn);
		}

		return;
	}

	// new connection
	pConn->m_status = NetConn::eStatus::Accepted;
	pConn->m_send.lastSendTime = QCGetCounter();

	int localAddrLen = 0;
	int remoteAddrLen = 0;
	lpfnGetAcceptExSockaddrs(
		pConn->m_addrBuffer,
		pAccept->bytesReceived,
		ADDR_SIZE, ADDR_SIZE,
		&pConn->m_pLocalAddr, &localAddrLen,
		&pConn->m_pRemoteAddr, &remoteAddrLen
	);
	SOCKADDR_IN* pLocalAddr = (SOCKADDR_IN*)pConn->m_pLocalAddr;
	pLocalAddr->sin_port = ntohs(pLocalAddr->sin_port);
	SOCKADDR_IN* pRemoteAddr = (SOCKADDR_IN*)pConn->m_pRemoteAddr;
	pRemoteAddr->sin_port = ntohs(pRemoteAddr->sin_port);

	// pendRecv()를 하기 전에 미리 PushJob() 한다. 
	// worker는 m_pConns를 직접 건들 수 없기 때문이다. 
	NetConnJob netConnJob;
	netConnJob.pConn = pConn;
	netConnJob.isNew = true;
	bool bRes = m_pQueue->PushJob(netConnJob);
	assert(bRes);

	bRes = pendRecv(pConn);
	if (bRes == false)
	{
		int refCount = 0;
		{
			LOCK_EXCLUSIVE(pConn->m_lock);
			disconnect(pConn);
			pConn->m_refCount--;
			refCount = pConn->m_refCount;
		}
		// pConn->m_lock 안에서 push를 해서는 안된다. 
		// worker thread는 push에 성공할 때까지 락을 들고 있는데, 
		// 만약 그 사이에 메인 스레드가 pConn->m_lock을 잡으면 데드락이 된다. 
		if (refCount == 0)
			pushDeletConnJob(pConn);
	}
}

void NetIO::handleRecv(NetConn* pConn, RecvOverlapped* pRecv, DWORD nBytes)
{
	assert(pConn != nullptr);
	assert(pRecv != nullptr);

	// 주의: 반드시 disconnect()를 해야 밑에서 pendReceive() 실패하고 refCount 감소 처리가 발생한다. 
	// 0 byte read 후에도 WSARecv()가 성공할 수 있기 때문이다. 
	if (nBytes == 0)
	{
		LOCK_EXCLUSIVE(pConn->m_lock);
		disconnect(pConn);
	}

	bool bSuccess = pushCompletedPackets(pConn, nBytes);
	if (bSuccess == false)
	{
		int refCount = 0;
		{
			LOCK_EXCLUSIVE(pConn->m_lock);
			disconnect(pConn);
			pConn->m_refCount--;
			refCount = pConn->m_refCount;
		}
		if (refCount == 0)
			pushDeletConnJob(pConn);

		return;
	}

	// recv buffer의 writeIndex가 끝까지 도달한 경우 rewind 처리
	if (RECV_BUFFER_SIZE == pRecv->writeIndex)
	{
		int readIndex = pRecv->readIndex;
		int nWritten = pRecv->writeIndex - readIndex;
		memcpy(pRecv->buffer, &pRecv->buffer[readIndex], nWritten);

		pRecv->readIndex = 0;
		pRecv->writeIndex = nWritten;

		assert(pRecv->writeIndex < RECV_BUFFER_SIZE);	// PushPacket()을 성공했으니 빈 자리가 있어야만 한다. 
	}

	// pend recv again
	bool bRes = pendRecv(pConn);
	if (bRes == false)
	{
		int refCount = 0;
		{
			LOCK_EXCLUSIVE(pConn->m_lock);
			disconnect(pConn);
			pConn->m_refCount--;
			refCount = pConn->m_refCount;
		}
		// pConn->m_lock 안에서 push를 해서는 안된다. 
		// worker thread는 push에 성공할 때까지 락을 들고 있는데, 
		// 만약 그 사이에 메인 스레드가 pConn->m_lock을 잡으면 데드락이 된다. 
		if (refCount == 0)
			pushDeletConnJob(pConn);
	}
}

void NetIO::handleSend(NetConn* pConn, SendOverlapped* pSend, DWORD nBytes)
{
	assert(pConn != nullptr);
	assert(pSend != nullptr);

	SendOverlapped& send = pConn->m_send;

	int refCount = 0;
	eSendResult eRes = eSendResult::None;
	bool isInFlushList = false;
	LARGE_INTEGER lastSendTime;
	// 한 번만 실행되는 loop. 
	// 더 이상 진행할 필요 없을 떄 break 해서 scoped lock에서 벗어나기 위해 사용
	// loop 안에서 위 변수들의 값을 결정한 후 빠져나와 핸들링한다. 
	while (true)
	{
		LOCK_EXCLUSIVE(send.lock);
		{
			LOCK_EXCLUSIVE(pConn->m_lock);
			if (nBytes == 0 || (pConn->m_socket == INVALID_SOCKET))
			{
				disconnect(pConn);
				if (pConn->m_refCount == 0)		// PostQueuedCompletionStatus
					assert(send.isSending == false);
				else
					pConn->m_refCount--;

				send.isSending = false;
			}
			refCount = pConn->m_refCount;
		}
		if (refCount == 0)
			break;
		if (send.isSending == false)
			break;

		bool bFlush = send.bFlush;
		send.bFlush = false;

		SendBuffer& sendBuffer = pSend->sendBuffer;
		bool bRes = sendBuffer.ReadDone(nBytes);
		// assert(bRes == true);
		// TEST: assert level의 에러지만 disconnect 시키고 계속한다. 
		if (bRes == false)
		{
			printf("[info]read done error: %d \n", pConn->GetID());

			eRes = eSendResult::PendFailed;
			{
				LOCK_EXCLUSIVE(pConn->m_lock);
				disconnect(pConn);
				send.isSending = false;
				pConn->m_refCount--;
				refCount = pConn->m_refCount;
			}
		 
			break;
		}

		// pendSend() 호출 여부 확인
		WSABUF buffers[2] = { 0 };
		SIZE_T readableSize = send.sendBuffer.Peek(&buffers[0], &buffers[1]);
		if (readableSize == 0)
		{
			eRes = eSendResult::NothingToSend;
			{
				LOCK_EXCLUSIVE(pConn->m_lock);
				send.isSending = false;
				pConn->m_refCount--;
				refCount = pConn->m_refCount;
			}

			break;
		}
		
		LARGE_INTEGER now = QCGetCounter();
		float dt = QCMeasureElapsedTick(now, send.lastSendTime);
		bool cond1 = readableSize >= SEND_THRESHOLD_SIZE;
		bool cond2 = dt >= SEND_THRESHOLD_TIME;
		bool cond3 = bFlush;
		bool bPend = cond1 || cond2 || cond3;
		if (bPend == false)
		{
			eRes = eSendResult::UnderThreshold;
			{
				isInFlushList = send.bToBeFlushed;
				send.bToBeFlushed = true;

				LOCK_EXCLUSIVE(pConn->m_lock);
				send.isSending = false;
				pConn->m_refCount--;

				refCount = pConn->m_refCount;
			}
			lastSendTime = send.lastSendTime;

			break;
		}

		bRes = pendSend(now, pConn, buffers);
		if (bRes == false)
		{
			eRes = eSendResult::PendFailed;
			{
				LOCK_EXCLUSIVE(pConn->m_lock);
				disconnect(pConn);
				send.isSending = false;
				pConn->m_refCount--;
				refCount = pConn->m_refCount;
			}

			break;
		}

		eRes = eSendResult::SendPended;

		break;
	}	// end of : while (true)

	// handle result 
	if (refCount == 0)
	{
		pushDeletConnJob(pConn);

		return;
	}
	else if (eRes == eSendResult::UnderThreshold)
	{
		if (isInFlushList == false)
		{
			LOCK_EXCLUSIVE(m_flushListLock);
			LARGE_INTEGER now = QCGetCounter();
			pSend->flushThreshold = QCCounterAddTick(now, SEND_THRESHOLD_TIME);
			AppendToList(&m_flushList, &pSend->flushLink);
		}

		return;
	}
}

SOCKET NetIO::createNonblockingSocket(ADDRINFO* pAddrinfo, bool bNagle)
{
	assert(pAddrinfo != nullptr);

	DWORD socketFlag = WSA_FLAG_OVERLAPPED;
	SOCKET ret = WSASocket(pAddrinfo->ai_family, pAddrinfo->ai_socktype, pAddrinfo->ai_protocol, nullptr, 0, socketFlag);
	if (ret == INVALID_SOCKET)
	{
		LOG_ERROR_STOP("WSASocket failed. LastError: %d", WSAGetLastError());

		return INVALID_SOCKET;
	}

	// duel stack
	int ipv6_only = 0;
	setsockopt(ret, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&ipv6_only, sizeof(ipv6_only));
	assert(iRes == NO_ERROR);

	unsigned long nonBlocking = 1ul;
	int iRes = ioctlsocket(ret, FIONBIO, &nonBlocking);
	if (iRes != NO_ERROR)
	{
		LOG_ERROR_STOP("ioctlsocket failed. LastError: %d", WSAGetLastError());

		return INVALID_SOCKET;
	}

	// bNagle == true: TCP_NO_DELAY를 끈다. bNagle == false: TCP_NODELAY 옵션을 킨다. 
	DWORD dwNagle = bNagle;
	iRes = setsockopt(ret, IPPROTO_TCP, TCP_NODELAY, (char*)&dwNagle, sizeof(dwNagle));
	assert(iRes == NO_ERROR);

	// linger: l_onoff가 1이면 closesocket() 바로 리턴하지 않고 liger 초만큼 대기. default: 0
	// LINGER linger;
	// linger.l_onoff = 1;	// linger 옵션 사용 여부
	// linger.l_linger = 5;
	// setsockopt(connPtr, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(LINGER));

	// SO_SNDBUF: 송신 버퍼 크기
	// SO_RCVBUF: 수신 버퍼 크기

	// SO_REUSEADDR: IP 주소 및 port 재사용
	int reuseOpt = 1;
	iRes = setsockopt(ret, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseOpt, sizeof(reuseOpt));
	assert(iRes == NO_ERROR);

	//// SOL_KEEPALIVE: 주기적으로 연결 상태 확인(TCP Ony). 끊어진 연결 감지
	//int keepAlive = 1;
	//iRes = setsockopt(ret, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepAlive, sizeof(keepAlive));
	//assert(iRes == NO_ERROR);

	//int keepIdle = 10;		// 첫번째 KeepAlive 패킷 전송까지 대기 시간 in sec
	//iRes = setsockopt(ret, IPPROTO_TCP, TCP_KEEPIDLE, (char*)&keepIdle, sizeof(keepIdle));
	//assert(iRes == NO_ERROR);

	//int keepInterval = 10;	// KeepAlive 패킷 간격 in sec
	//iRes = setsockopt(ret, IPPROTO_TCP, TCP_KEEPINTVL, (char*)&keepInterval, sizeof(keepInterval));
	//assert(iRes == NO_ERROR);

	//int keepCount = 1;		// 시도 횟수. 1이면 최초 실패 시 바로 접속 해제 처리한다. 
	//iRes = setsockopt(ret, IPPROTO_TCP, TCP_KEEPCNT, (char*)&keepCount, sizeof(keepCount));
	//assert(iRes == NO_ERROR);

	// #include <mstcpip.h>  // SIO_KEEPALIVE_VALS
	// tcp_keepalive keepAliveDesc;
	// keepAliveDesc.onoff = TRUE;
	// keepAliveDesc.keepalivetime = keepIdle * SEC_TO_MS;
	// keepAliveDesc.keepaliveinterval = keepInterval * SEC_TO_MS;
	// 재시도 횟수 설정은 없음
	// DWORD bytesReturned = 0;
	// iRes = WSAIoctl(
	// 	ret, SIO_KEEPALIVE_VALS, &keepAliveDesc, sizeof(keepAliveDesc), NULL, 0, &bytesReturned, NULL, NULL
	// );
	// assert(iRes == NO_ERROR);

	return ret;
}

bool NetIO::pendRecv(NetConn* pConn)
{
	assert(pConn != nullptr);

	RecvOverlapped& recv = pConn->m_recv;
	WSABUF wsaBuf;
	wsaBuf.buf = &recv.buffer[recv.writeIndex];
	wsaBuf.len = RECV_BUFFER_SIZE - recv.writeIndex;
	DWORD flags = 0;
	DWORD nBytes = 0;
	int iRes = WSARecv(pConn->m_socket, &wsaBuf, 1, &nBytes, &flags, &recv, nullptr);
	if (iRes == NO_ERROR)
		return true;
	int iError = WSAGetLastError();
	if (iError != WSA_IO_PENDING)
	{
		if (iError == WSAECONNRESET)					// closed by remote host, not gracefully
			return false;
		else if (iError == ERROR_CONNECTION_ABORTED)	// aborted by local system
			return false;
		else if (iError == WSAECONNABORTED)
			return false;
		else if (iError == WSAENOTCONN)		// connect() 실패
			return false;
		else if (iError == WSAENOTSOCK)		// disconnect() 처리된 소켓
		{
#ifdef _DEBUG
			LOCK_EXCLUSIVE(pConn->m_lock);	// 락 안걸면 에러
			assert(pConn->m_socket == INVALID_SOCKET);
#endif // _DEBUG
			
			return false;
		}
		else if (iError == WSAEINTR)
		{
			// 유니티 더미 클라로 테스트할 때
			// Start 버튼 Connect 완료하고 순차적으로 누르지 않고
			// 기다리지 않고 바로 연속적으로 Connect Connect 하니까 발생

			LOG_ERROR("WSAReceive() failed.LastError: WSAEINTR");

			return false;
		}
		else    // 뭘까 
			LOG_ERROR_STOP("WSAReceive() failed.LastError: %d ", iError);

		return false;
	}

	return true;
}

bool NetIO::pushCompletedPackets(NetConn* pConn, DWORD nbytes)
{
	assert(pConn != nullptr);

	RecvOverlapped& recv = pConn->m_recv;
	recv.writeIndex += nbytes;

	while (true)
	{
		void* pRead = &recv.buffer[recv.readIndex];
		int readableSize = recv.writeIndex - recv.readIndex;
		if (readableSize == 0)
			return true;

		RangedReader reader(pRead, readableSize);

		USHORT payloadSize = 0;
		void* pRes = reader.ReadUShort(&payloadSize);
		if (pRes == nullptr)					// 1바이트가 도착했나? 
			return true;
		if (payloadSize < 2)					// 너무 작은 경우	
			return false;
		if (payloadSize > MAX_PAYLOAD_SIZE)		// 너무 큰 경우
			return false;
		if (readableSize < sizeof(USHORT) + payloadSize)	// 패킷(payload size + payload)이 다 도착하지 않은 경우
			return true;

		// 패킷 완성 확인
		SenderID senderID = pConn->GetID();
		void* pPackedPacket = reader.ReadRaw(0, false);
		bool bRes = m_pQueue->PushPacket(senderID, pPackedPacket, payloadSize);
		assert(bRes == true);

		recv.readIndex += sizeof(USHORT) + payloadSize;
	}

	return true;
}

eSendResult NetIO::send(NetConn* pConn, Packet* pPacket, bool bFlush)
{
	assert(pConn != nullptr);

	// 락을 걸지 않고 미리 확인: 어차피 아래서 락 걸고 또 확인하긴 한다. 
	// 그래도 미리 확인하면 SendBuffer::Write()를 호출하지 않고 리턴할 수 있다. 
	if (pConn->m_socket == INVALID_SOCKET)		// 이미 disconnect됨
		return eSendResult::Disconnected;

	SendOverlapped& send = pConn->m_send;
	// LOCK_EXCLUSIVE(send.lock);				// 외부에서 락을 걸어야 한다. 

	// sendBuffer에 packet write
	if (pPacket != nullptr)
	{
		SendBuffer& sendBuffer = send.sendBuffer;
		bool bWriteRes = sendBuffer.Write(*pPacket);
		if (bWriteRes == false)
		{
			printf("[info]sendbuffer exceeded \n");
			printf(" - connID: %d \n", pConn->GetID());

			LOCK_EXCLUSIVE(pConn->m_lock);
			disconnect(pConn);

			return eSendResult::Overflow;
		}
	}

	// pendSend() 호출 여부 확인1
	if (send.isSending)		// 이미 pend된 WSASend()가 있음으로 또 pend하지 않는다. 
	{
		if (bFlush)			// 이미 pend된 WSASend() 완료 후 강제로 pendSend 하도록 설정
			send.bFlush = true;

		return eSendResult::AlreadySending;
	}

	// send.isSending == false

	// pendSend() 호출 여부 확인2
	WSABUF buffers[2] = { 0 };
	SIZE_T readableSize = send.sendBuffer.Peek(&buffers[0], &buffers[1]);
	if (readableSize == 0)
		return eSendResult::NothingToSend;

	LARGE_INTEGER now = QCGetCounter();
	// float dt = QCMeasureElapsedTick(now, send.lastSendTime);
	// bool cond2 = dt >= SEND_THRESHOLD_TIME;
	bool cond1 = readableSize >= SEND_THRESHOLD_SIZE;
	bool cond3 = bFlush;
	bool bPend = cond1 || cond3;	// cond1 || cond2 || cond3;
	if (!bPend)
		return eSendResult::UnderThreshold;

	{
		LOCK_EXCLUSIVE(pConn->m_lock);

		// pendSend() 호출 여부 확인3
		if (pConn->m_socket == INVALID_SOCKET)		// 이미 disconnect됨
			return eSendResult::Disconnected;

		pConn->m_refCount++;
	}

	// finally, pendSend
	send.isSending = true;
	bool bRes = pendSend(now, pConn, buffers);
	if (bRes == false)
	{
		int refCount = 0;
		{
			LOCK_EXCLUSIVE(pConn->m_lock);
			disconnect(pConn);
			pConn->m_refCount--;
			refCount = pConn->m_refCount;
		}
		send.isSending = false;
		if (refCount == 0)	// msgQueue 용량 부족할 때 pushDeletConnJob() 하면 실패한다. 
		{
			bool bRes = PostQueuedCompletionStatus(m_iocp, 0, (ULONG_PTR)pConn, &send);
			assert(bRes == true);
		}
		
		return eSendResult::PendFailed;
	}

	return eSendResult::SendPended;
}

bool NetIO::pendSend(LARGE_INTEGER now, NetConn* pConn, WSABUF buffers[2])
{
	assert(pConn != nullptr);
	assert(buffers != nullptr);

	SendOverlapped& send = pConn->m_send;

	DWORD flags = 0;
	int iRes = WSASend(pConn->m_socket, buffers, 2, nullptr, flags, &send, nullptr);
	if (iRes == NO_ERROR)
	{
		send.lastSendTime = now;

		return true;
	}
	int iError = WSAGetLastError();
	if (iError != WSA_IO_PENDING)
	{
		bool isKnown =
			(iError == WSAECONNRESET) ||				// send 사이에 접속이 끊긴 경우
			(iError != ERROR_CONNECTION_ABORTED);		// 서버가 closesocket
		if (isKnown == false)
			LOG_ERROR_STOP("WSASend(() failed.LastError: %d \n", iError);			// 뭘까.

		return false;
	}

	// iError == WSA_IO_PENDING
	send.lastSendTime = now;

	return true;
}

void NetIO::disconnect(NetConn* pConn)
{
	assert(pConn);

	if (pConn->m_socket == INVALID_SOCKET)
		return;

	SOCKET socket = pConn->m_socket;	// backup
	pConn->m_socket = INVALID_SOCKET;	// set invalid first
	shutdown(socket, SD_BOTH);
	closesocket(socket);
}

void NetIO::pushDeletConnJob(NetConn* pConn)
{
	assert(pConn != nullptr);
	assert(pConn->m_refCount == 0);

	NetConnJob job;
	job.pConn = pConn;
	job.isNew = false;
	bool bRes = m_pQueue->PushJob(job);
	assert(bRes == true);
}

bool NetIO::executeNetConnJob(NetConn* pConn, bool isNew)
{
	bool bRet = true;
	if (isNew)
	{
		assert(pConn->m_status == NetConn::eStatus::Accepted);

		pConn->m_status = NetConn::eStatus::InUse;
		bRet = m_callback(pConn, true);
	}
	else
	{
		assert(pConn->m_status == NetConn::eStatus::InUse);
		assert(pConn->m_refCount == 0);

		bRet = m_callback(pConn, false);

		{
			SendOverlapped& sendOverlapped = pConn->m_send;
			// refCount가 없고 더 이상 접근되지 않음으로 SendOverlapped에 락은 걸지 않아도 된다. 
			// LOCK_EXCLUSIVE(sendOverlapped.lock);
			if (sendOverlapped.bToBeFlushed)
			{
				sendOverlapped.bToBeFlushed = false;
				LOCK_EXCLUSIVE(m_flushListLock);
				UnlinkFromList(&m_flushList, &sendOverlapped.flushLink);
			}

			if (sendOverlapped.bToBeUrgentFlushed)
			{
				sendOverlapped.bToBeUrgentFlushed = false;
				LOCK_EXCLUSIVE(m_urgentListLock);
				UnlinkFromList(&m_urgentList, &sendOverlapped.urgentLink);
			}
		}

		// ID를 반납하고 connection을 삭제한다. 
		NetConnID id = pConn->m_id;
		m_pConns[id % MAX_CONNECTION] = nullptr;
		{
			LOCK_EXCLUSIVE(m_connPoolLock);
			m_connPool.Free(pConn);
			// delete pConn;
		}
	}

	return bRet;
}
#pragma endregion

#pragma region NETCONN_JOB
NetConnJob::NetConnJob()
	: Job(GetPackableInfo<NetConnJob>())
	, pConn(nullptr)
	, isNew(false)
{}

bool NetConnJob::Execute()
{
	NetIO* pNetIO = pConn->GetNetIO();
	bool bRet = pNetIO->executeNetConnJob(pConn, isNew);

	return bRet;
}

bool NetConnJob::packField(IWriter* pWriter)
{
	if (pWriter->WritePointer((void*)pConn) == nullptr)
		return false;
	if (pWriter->WriteBool(isNew) == nullptr)
		return false;

	return true;
}

bool NetConnJob::unpackField(IReader* pReader)
{
	if (pReader->ReadPointer((void**)&pConn) == nullptr)
		return false;
	if (pReader->ReadBool(&isNew) == nullptr)
		return false;
	assert(pReader->GetReadableSize() == 0);

	return true;
}
#pragma endregion
