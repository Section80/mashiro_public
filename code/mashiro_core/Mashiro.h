#pragma once

#pragma comment(lib, "ws2_32.lib")

#include "network/NetIO.h"
#include "buffer/HashMap.h"
#include "SqlHelper.h"

// 1. call QCInit()
// 2. _wsetlocale(LC_ALL, L"");
// 3. call WSAStartup
// 4. register default packables
//     - starts 60000
bool InitMashro();
void CleanUpMashiro();		// WSACleanup();

using OnPacket		= bool (*)(NetConn* pSender, PackableType opcode, IReader* packetReader);
using OnEvent		= bool (*)(PackableType eventType, Event* pEvent);

struct HandleMessageParam
{
	MsgQueue::Popped*	pPopped;		// not nullable
	NetIO*				pNetIO;			// nullable

	OnPacket			onPacket;		// nullable, requires not-null pNetIO to be called
	OnEvent				onEvent;		// nullable

};
using HandleMsgParam = HandleMessageParam;
bool HandleMessage(HandleMsgParam* pParam);
