#include <string.h>
#define __VER__ (strrchr(__FILE__, '_') ? strrchr(__FILE__, '_') + 1 : __FILE__)
#define _USE_LOGGER_

//	v.5.1
// 
//	5.0 + disconnect와 deleteSession을 위해 findSession에서 ioCount를 referenceCount처럼 사용해 
//	
//

#pragma comment(lib, "ws2_32")

#ifdef _DEBUG
#pragma comment(lib, "RingBufferD")
#pragma comment(lib, "MemoryPoolD")
#pragma comment(lib, "SerializerD")
#pragma comment(lib, "MessageLoggerD")
#pragma comment(lib, "crashDumpD")

#else
#pragma comment(lib, "RingBuffer")
#pragma comment(lib, "MemoryPool")
#pragma comment(lib, "Serializer")
#pragma comment(lib, "MessageLogger")
#pragma comment(lib, "crashDump")

#endif

#include "network.h"

#include <WinSock2.h>
#include <iostream>
#include <stack>
#include <unordered_map>

#include "RingBuffer/RingBuffer/RingBuffer.h"
#include "MemoryPool/MemoryPool/MemoryPool.h"
#include "Serializer/Serializer/Serializer.h"
#include "MessageLogger/MessageLogger/MessageLogger.h"
#include "crashDump/crashDump/crashDump.h"

#include "errorDefine.h"
#include "packet.h"
#include "session.h"

// writeMessageBuffer(const char* const _format, ...)



Network::Network() : sessionPool(3000){

};

Network::~Network()
{
	if (isRun)
		stop();

	HANDLE arr[USHRT_MAX];
	int threads = 0;

	arr[threads++] = hAcceptThread;
	CloseHandle(hAcceptThread);
	arr[threads++] = hNetworkThread;
	CloseHandle(hNetworkThread);

	for (auto it : threadIndexMap) {
		arr[threads++] = it.first;
		CloseHandle(it.first);
	}

	auto ret = WaitForMultipleObjects(threads, arr, true, 10 * 1000);
	if (ret == WAIT_TIMEOUT)
	{
		/////
		// 10초 내로 종료 실패, 추가 동작 정의 필요
		////
	}

	delete[] acceptTPSArr;
	delete[] recvMessageTPSArr;
	delete[] sendMessageTPSArr;

	delete[] sessionArray;
}

bool Network::start(const USHORT port, const UINT16 maxSessionSize,
	const UINT8 workerThreadSize, const UINT8 runningThreadSize,
	const int backLogSize, const ULONG host)
{
	workerThreadCount = workerThreadSize;

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		OnError(ERRORCODE_WSASTARTUP);
		return false;
	}

	listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_socket == INVALID_SOCKET) {
		OnError(ERRORCODE_SOCKET, "listen socket");
		return false;
	}

	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.S_un.S_addr = htonl(host);
	serverAddr.sin_port = htons(port);

	int ret = bind(listen_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	if (ret == SOCKET_ERROR) {
		OnError(ERRORCODE_BIND);
		return false;
	}

	ret = listen(listen_socket, SOMAXCONN_HINT(backLogSize));
	if (ret == SOCKET_ERROR) {
		OnError(ERRORCODE_LISTEN);
		return false;
	}

	{
		IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, runningThreadSize);
		if (IOCP == NULL) {
			writeMessageBuffer("CreateIoCompletionPort, GetLastError %d", GetLastError());
			OnError(ERRORCODE_CREATEKERNELOBJECT, messageBuffer);

			return false;
		}

		sessionCount = 0;
		maxSession = maxSessionSize;
		workerThreadCount = workerThreadSize;
		runningThreadCount = runningThreadSize;
		for (int i = maxSession - 1; i >= 0; i--)
			indexStack.push(i);

		sessionArray = new session * [maxSession];
		acceptTPSArr = new unsigned int[1];
		recvMessageTPSArr = new unsigned int[workerThreadCount];
		sendMessageTPSArr = new unsigned int[workerThreadCount];

		//sessionPool.init(maxSession);
		isRun = true;
	}

	hAcceptThread = CreateThread(NULL, 0, acceptThread, this, 0, NULL);
	if (hAcceptThread == NULL) {
		isRun = false;
		writeMessageBuffer("CreateThread, GetLastError %d, acceptThread", GetLastError());
		OnError(ERRORCODE_CREATEKERNELOBJECT, messageBuffer);

		return false;
	}

	HANDLE hThread;
	for (int i = 0; i < workerThreadCount; i++) {
		hThread = CreateThread(NULL, 0, workerThread, this, 0, NULL);
		if (hThread == NULL) {
			isRun = false;
			writeMessageBuffer("CreateThread, GetLastError %d, workerThread", GetLastError());
			OnError(ERRORCODE_CREATEKERNELOBJECT, messageBuffer);

			return false;
		}
		threadIndexMap[hThread] = i;
	}

	hNetworkThread = CreateThread(NULL, 0, networkThread, this, 0, NULL);
	if (hNetworkThread == NULL) {
		isRun = false;
		writeMessageBuffer("CreateThread, GetLastError %d, networkThread", GetLastError());
		OnError(ERRORCODE_CREATEKERNELOBJECT, messageBuffer);

		return false;
	}

	return true;
}

void Network::stop() {
	isRun = false;
	if (listen_socket != INVALID_SOCKET)
		closesocket(listen_socket);

	if (IOCP != nullptr)
	{
		for (int i = 0; i < threadIndexMap.size(); i++)
			PostQueuedCompletionStatus(IOCP, 0, (ULONG_PTR)nullptr, nullptr);
		CloseHandle(IOCP);
	}

	//sessionPool.clear();

	WSACleanup();
}

DWORD WINAPI Network::networkThread(LPVOID arg)
{
	Network* core = ((Network*)arg);

	while (core->isRun)
	{
		Sleep(2000);

		unsigned int temp1 = 0;
		unsigned int temp2 = 0;
		for (int i = 0; i < core->workerThreadCount; i++) {
			temp1 += core->sendMessageTPSArr[i];
			core->sendMessageTPSArr[i] = 0;
			temp2 += core->recvMessageTPSArr[i];
			core->recvMessageTPSArr[i] = 0;
		}
		core->sendMessageTPS = temp1 / core->workerThreadCount;
		core->recvMessageTPS = temp2 / core->workerThreadCount;
		core->acceptTPS = core->acceptTPSArr[0];
		core->acceptTPSArr[0] = 0;
	}

	return 0;
}

DWORD WINAPI Network::acceptThread(LPVOID arg) {
	Network* core = ((Network*)arg);
	core->sessionPool.init();
	serializerPool.init();

	while (core->isRun)
	{
		SOCKET client_sock = accept(core->listen_socket, NULL, NULL);
		core->acceptTPSArr[0]++;

		if (client_sock == INVALID_SOCKET)
			continue;

		SOCKADDR_IN clientAddr;
		int addrlen = sizeof(clientAddr);
		getpeername(client_sock, (SOCKADDR*)&clientAddr, &addrlen);

		if (!core->OnConnectionRequest(clientAddr.sin_addr.S_un.S_addr, clientAddr.sin_port)) {
			closesocket(client_sock);
			continue;
		}

		AcquireSRWLockExclusive(&(core->indexStackLock));
		int freeSessionIDSize = core->indexStack.size();
		ReleaseSRWLockExclusive(&(core->indexStackLock));
		if (freeSessionIDSize == 0) {
			closesocket(client_sock);
			continue;
		}

		linger l = { 0, 0 };
		setsockopt(client_sock, SOL_SOCKET, SO_LINGER, (char*)&l, sizeof(l));

		int optVal = 0;
		//setsockopt(client_sock, SOL_SOCKET, SO_SNDBUF, (char*)&optVal, sizeof(optVal));

		int nagleOpt = TRUE;
		setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&nagleOpt, sizeof(nagleOpt));

		AcquireSRWLockExclusive(&(core->sessionPoolLock));
		session* sessionPtr = core->sessionPool.Alloc();
		ReleaseSRWLockExclusive(&(core->sessionPoolLock));

		// 세션 ID 부여 및 초기화
		{
			AcquireSRWLockExclusive(&(core->indexStackLock));
			USHORT index = core->indexStack.top();
			core->indexStack.pop();
			ReleaseSRWLockExclusive(&(core->indexStackLock));

			core->sessionArray[index] = sessionPtr;

			UINT64 sessionID = MAKE_SESSION_ID(core->sessionCount++, index);
			sessionPtr->init(sessionID, client_sock, clientAddr.sin_port);


			CreateIoCompletionPort((HANDLE)(sessionPtr->getSocket()), core->IOCP, (ULONG_PTR)sessionPtr, NULL);
			core->OnNewConnect(sessionID);
		}



		if (!sessionPtr->recvIO())
		{
			if (sessionPtr->decrementIO() == 0)
				core->deleteSession(sessionPtr);
		}
	}

	core->sessionPool.clear();
	serializerPool.clear();
	return 0;
}

DWORD WINAPI Network::workerThread(LPVOID arg)
{
	Network* core = ((Network*)arg);
	SHORT threadIndex = core->threadIndexMap[GetCurrentThread()];

	DWORD transfer;
	session* sessionPtr;
	OVERLAPPED* overlap;
	overlapped* o;
	core->sessionPool.init();
	serializerPool.init();
	
	while (core->isRun)
	{

		bool GQCSresult = GetQueuedCompletionStatus(core->IOCP, &transfer, (PULONG_PTR)&sessionPtr, &overlap, INFINITE);
		if (sessionPtr == nullptr) //PQCS 에 의한 종료
			break;

		o = ((overlapped*)overlap);
		if (GQCSresult == false || transfer == 0)
		{
			if (sessionPtr->decrementIO() == 0)
				core->deleteSession(sessionPtr);

			continue;
		}

		if (sessionPtr->sendOverlappedCheck(o))	//send 완료 블록
		{
			core->OnSend(sessionPtr->ID, transfer);
			int tp = sessionPtr->sended(transfer);
			if (tp != -1)
				core->sendMessageTPSArr[threadIndex] += tp;
			else
			{
				//
				// sended -1 예외처리
				//
			}

			InterlockedExchange(&sessionPtr->sendFlag, 0);
			// 이순간에는 IOCount 차감 안되있음, sendFlag 꺼져있음 상태
			// 
			
			//sessionPtr->sendIO();
			while (InterlockedCompareExchange(&(sessionPtr->sendFlag), 0, 0) == 0 && !(sessionPtr->sendBuffer.empty()))
			{
				if (!sessionPtr->sendIO())
				{
					if (sessionPtr->decrementIO() == 0) {
						core->deleteSession(sessionPtr);
						break;
					}
				}
			}

		}
		else	//recv 완료 블록
		{
			if (!(sessionPtr->recved(transfer)))
			{
				// transfer 크기가 안맞거나, 버퍼 오버플로우 등 정상적으로 수신 처리 불가능할때
				if (sessionPtr->decrementIO() == 0) {
					core->deleteSession(sessionPtr);
					continue;
				}
			}

			while (true) {
				packet p;
				if (!sessionPtr->recvedPacket(p))
					break;

				core->recvMessageTPSArr[threadIndex]++;
				core->OnRecv(sessionPtr->ID, p);
			}

			//sessionPtr->incrementIO();
			//if (!sessionPtr->recvIO())
			//{
			//	if (sessionPtr->decrementIO() == 0) {
			//		core->deleteSession(sessionPtr);
			//	}
			//}

			if (sessionPtr->recvIO())
				sessionPtr->incrementIO();
		}

		if (sessionPtr->decrementIO() == 0)
			core->deleteSession(sessionPtr);
	}
	core->sessionPool.clear();
	serializerPool.clear();
	return 0;
}

void Network::deleteSession(session* sessionPtr)
{
	//writeMessageBuffer("disconnect %d", info);
	//LOG(logLevel::Info, LO_TXT, "123");

	USHORT index = (USHORT)sessionPtr->ID;

	closesocket(sessionPtr->socket);

	sessionPtr->bufferClear();
	AcquireSRWLockExclusive(&sessionPoolLock);
	sessionPool.Free(sessionPtr);
	ReleaseSRWLockExclusive(&sessionPoolLock);

	AcquireSRWLockExclusive(&indexStackLock);
	indexStack.push(index);
	ReleaseSRWLockExclusive(&indexStackLock);
}

size_t Network::getSessionCount()
{
	AcquireSRWLockExclusive(&indexStackLock);
	size_t ret = maxSession - indexStack.size();
	ReleaseSRWLockExclusive(&indexStackLock);

	return ret;
}

std::pair<size_t, size_t> Network::getSessionPoolMemory()
{
	pair<size_t, size_t> ret;

	AcquireSRWLockExclusive(&sessionPoolLock);
	ret.first = sessionPool.GetCapacityCount();
	ret.second = sessionPool.GetUseCount();
	ReleaseSRWLockExclusive(&sessionPoolLock);

	return ret;
}

unsigned int Network::getAcceptTPS()
{
	return acceptTPS;
}
unsigned int Network::getRecvMessageTPS()
{
	return recvMessageTPS;
}
unsigned int Network::getSendMessageTPS()
{
	return sendMessageTPS;
}

////////////////////////////////////////////////////////////////////////////////////
// 이하 컨텐츠단에서 먼저 호출 가능한 함수들
////////////////////////////////////////////////////////////////////////////////////
sessionPtr Network::findSession(UINT64 sessionID)
{
	session* s = sessionArray[(USHORT)sessionID];

	if (s->ID != sessionID)
		return sessionPtr(nullptr, this);

	return sessionPtr(s, this);
}

bool Network::sendPacket(UINT64 sessionID, packet& _pakcet)
{
	sessionPtr s = findSession(sessionID);

	if (s.ptr == nullptr)
		return false;

	if (s.ptr->collectSendPacket(_pakcet))
	{
		if (!(s.ptr->sendIO()))
		{
			if (s.ptr->decrementIO() == 0) {
				deleteSession(s.ptr);
				return false;
			}
		}
	}

	return true;
}
