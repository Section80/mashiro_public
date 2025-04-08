#include "Mashiro.h"

constexpr int MSG_BUFFER_SIZE = 2048;

bool InitMashro()
{
	QCInit();
	_wsetlocale(LC_ALL, L"");

	WSADATA wsaData;
	int iRes = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iRes != 0)
	{
		// A call to the WSAGetLastError function is not needed and should not be used.
		LOG_ERROR_STOP("WSAStartup failed. ret: %d", iRes);
		return false;
	}

	REGISTER_PACKABLE_TYPE(NetConnJob, 60000);

	return true;
}

void CleanUpMashiro()
{
	WSACleanup();
}

bool HandleMessage(HandleMsgParam* pParam)
{
	assert(pParam != nullptr);

	bool bRet = true;

	thread_local static BYTE msgBuffer[2048] = { 0 };
	MsgQueue::Popped* pPopped = pParam->pPopped;
	NetIO* pNetIO = pParam->pNetIO;
	if (pPopped->messageType == eMessageType::Event)
	{
		OnEvent onEvent = pParam->onEvent;
		if (onEvent == nullptr)
			return true;

		// peek packableInfo to get initFunc
		PackableInfo* pInfo = nullptr;
		void* pRes = pPopped->messageReader.PeekPointer((void**)pInfo);
		assert(pRes != nullptr);
		assert(pInfo != nullptr);

		// placement new on msgBuffer
		Event* pEvent = (Event*)pInfo->initFunc(msgBuffer, MSG_BUFFER_SIZE);
		assert(pEvent != nullptr);

		// deserialize members
		bool bRes = pEvent->Unpack(&pPopped->messageReader);
		assert(bRes == true);

		bRet = onEvent(pPopped->packableType, pEvent);
		pEvent->~Event();	// call detor manually. e.g. std::string, ... 

		return bRet;
	}
	else if (pPopped->messageType == eMessageType::Job)
	{
		// peek packableInfo to get initFunc
		PackableInfo* pInfo = nullptr;
		void* pRes = pPopped->messageReader.PeekPointer((void**)&pInfo);
		assert(pRes != nullptr);
		assert(pInfo != nullptr);

		// placement new on msgBuffer
		Job* pJob = (Job*)pInfo->initFunc(msgBuffer, MSG_BUFFER_SIZE);
		assert(pJob != nullptr);

		// deserialize members
		bool bRes = pJob->Unpack(&pPopped->messageReader);
		assert(bRes == true);

		bRet = pJob->Execute();
		pJob->~Job();	// call detor manually. e.g. std::string, ... 

		return bRet;
	}
	else if (pPopped->messageType == eMessageType::Packet)
	{
		OnPacket onPacket = pParam->onPacket;
		if (onPacket == nullptr)
			return true;
		if (pNetIO == nullptr)
			return true;

		SenderID senderID = pPopped->senderID;
		NetConn* pConn = pNetIO->GetConn(senderID);
		if (pConn == nullptr)	// disconn already handled
			return true;

		// in case of packet, user should call packet.Unpack() self to handle invalid packets
		bRet = onPacket(pConn, pPopped->packableType, &pPopped->messageReader);
		// packets should not require dtor
		
		return bRet;
	}

	// pPopped->messageType == eMessageType::None

	return true;
}
