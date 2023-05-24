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
#pragma comment(lib, "MemoryPoolD")
#pragma comment(lib, "MessageLoggerD")
#pragma comment(lib, "crashDumpD")

#else
#pragma comment(lib, "MemoryPool")
#pragma comment(lib, "MessageLogger")
#pragma comment(lib, "crashDump")

#endif

#include "network.h"

#include <WinSock2.h>
#include <iostream>
#include <stack>
#include <unordered_map>

#include "MemoryPool/MemoryPool/MemoryPool.h"
#include "MessageLogger/MessageLogger/MessageLogger.h"
#include "crashDump/crashDump/crashDump.h"

#include "errorDefine.h"
#include "packet.h"
#include "session.h"

// writeMessageBuffer(const char* const _format, ...)



Network::Network() : sessionPool(3000) {

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

	linger l = { 0, 0 };
	if (setsockopt(listen_socket, SOL_SOCKET, SO_LINGER, (char*)&l, sizeof(l)) != 0)
		LOG(logLevel::Error, LO_TXT, "Error in set Linger " + to_string(listen_socket) + " socket, error : " + to_string(GetLastError()));

	int optVal = 0;
	//setsockopt(client_sock, SOL_SOCKET, SO_SNDBUF, (char*)&optVal, sizeof(optVal));

	int nagleOpt = TRUE;
	if (setsockopt(listen_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&nagleOpt, sizeof(nagleOpt)) != 0)
		LOG(logLevel::Error, LO_TXT, "Error in set Nodelay" + to_string(listen_socket) + " socket, error : " + to_string(GetLastError()));

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
		memset(sessionArray, 0, sizeof(session*) * maxSession);
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

	while (core->isRun)
	{
		SOCKET client_sock = accept(core->listen_socket, NULL, NULL);
		core->acceptTPSArr[0]++;

		if (client_sock == INVALID_SOCKET)
			continue;
		//LOG(logLevel::Info, LO_TXT, "new connect " + to_string(client_sock) + " socket");

		SOCKADDR_IN clientAddr;
		int addrlen = sizeof(clientAddr);
		if(getpeername(client_sock, (SOCKADDR*)&clientAddr, &addrlen) != 0)
			LOG(logLevel::Error, LO_TXT, "Error in getpeername " + to_string(client_sock) + " socket, error : " + to_string(GetLastError()));

		if (!core->OnConnectionRequest(clientAddr.sin_addr.S_un.S_addr, clientAddr.sin_port)) {
			LOG(logLevel::Error, LO_TXT, "OnConnectionRequest fail");
			closesocket(client_sock);
			continue;
		}

		//{
		//	linger l = { 0, 0 };
		//	if (setsockopt(client_sock, SOL_SOCKET, SO_LINGER, (char*)&l, sizeof(l)) != 0)
		//		LOG(logLevel::Error, LO_TXT, "Error in set Linger " + to_string(client_sock) + " socket, error : " + to_string(GetLastError()));
		//
		//	int optVal = 0;
		//	//setsockopt(client_sock, SOL_SOCKET, SO_SNDBUF, (char*)&optVal, sizeof(optVal));
		//
		//	int nagleOpt = TRUE;
		//	if (setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&nagleOpt, sizeof(nagleOpt)) != 0)
		//		LOG(logLevel::Error, LO_TXT, "Error in set Nodelay" + to_string(client_sock) + " socket, error : " + to_string(GetLastError()));
		//}

		USHORT index = -1;

		AcquireSRWLockExclusive(&(core->indexStackLock));
		if (core->indexStack.size() == 0) {
			LOG(logLevel::Error, LO_TXT, "freeSessionIDSize empty fail");
			closesocket(client_sock);
			ReleaseSRWLockExclusive(&(core->indexStackLock));
			continue;
		}
		else
		{
			index = core->indexStack.top();
			core->indexStack.pop();
		}
		ReleaseSRWLockExclusive(&(core->indexStackLock));

		
		//AcquireSRWLockExclusive(&(core->sessionPoolLock));
		session* sessionPtr = core->sessionPool.Alloc();
		//ReleaseSRWLockExclusive(&(core->sessionPoolLock));
		sessionPtr->bufferClear();
		// 세션 ID 부여 및 초기화
		{
			core->sessionArray[index] = sessionPtr;

			UINT64 sessionID = MAKE_SESSION_ID(core->sessionCount++, index);
			sessionPtr->init(sessionID, client_sock, clientAddr.sin_port);


			if(CreateIoCompletionPort((HANDLE)(sessionPtr->getSocket()), core->IOCP, (ULONG_PTR)sessionPtr, NULL) == NULL)
				LOG(logLevel::Error, LO_TXT, "Error in CreateIoCompletionPort " + to_string(sessionPtr->getSocket()) + " socket, error : " + to_string(GetLastError()));

			core->OnNewConnect(sessionID);
		}



		if (!sessionPtr->recvIO())
		{
			if (sessionPtr->decrementIO() == 0)
				core->deleteSession(sessionPtr);
		}
	}

	return 0;
}

UINT64 temp1Counter = 0;

DWORD WINAPI Network::workerThread(LPVOID arg)
{
	Network* core = ((Network*)arg);
	SHORT threadIndex = core->threadIndexMap[GetCurrentThread()];

	DWORD transfer;
	session* sessionPtr;
	OVERLAPPED* overlap;

	while (core->isRun)
	{

		bool GQCSresult = GetQueuedCompletionStatus(core->IOCP, &transfer, (PULONG_PTR)&sessionPtr, &overlap, INFINITE);
		if (sessionPtr == nullptr) //PQCS 에 의한 종료
			break;

		if (GQCSresult == false || transfer == 0)
		{
			if (sessionPtr->sendOverlappedCheck(overlap))	//send 완료 블록
			{
				LOG(logLevel::Info, LO_TXT, "disconnect send  session " + to_string(sessionPtr->ID));
			}
			else if (sessionPtr->recvOverlappedCheck(overlap))	//recv 완료 블록
			{
				LOG(logLevel::Info, LO_TXT, "disconnect recv  session " + to_string(sessionPtr->ID));
			}
			if (sessionPtr->decrementIO() == 0)
				core->deleteSession(sessionPtr);

			continue;
		}

		if (sessionPtr->sendOverlappedCheck(overlap))	//send 완료 블록
		{
			core->OnSend(sessionPtr->ID, transfer);
			int tp = sessionPtr->sended(transfer);
			if (tp != -1)
				core->sendMessageTPSArr[threadIndex] += tp;
			else
			{
				if (sessionPtr->decrementIO() == 0)
					core->deleteSession(sessionPtr);
				continue;
				//
				// sended -1 예외처리
				//
			}

			InterlockedExchange16(&sessionPtr->sendFlag, 0);

			if (sessionPtr->sendBuffer.size() > 0) // 잠재적 에러 가능 (size는 1 이상인데 실제 들어있는것은 없는 경우
			{
				if (!sessionPtr->sendIO()) {
					if (sessionPtr->decrementIO() == 0)
						core->deleteSession(sessionPtr);
				}
			}
		}
		else if (sessionPtr->recvOverlappedCheck(overlap))	//recv 완료 블록
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
		else
		{
			InterlockedIncrement(&temp1Counter);
			continue;
		}

		if (sessionPtr->decrementIO() == 0)
			core->deleteSession(sessionPtr);
	}
	return 0;
}

void Network::deleteSession(session* sessionPtr)
{
	//오규리 :: 질문 1) IO Count 내부에 있는 플래그는 누구? - IO를 취소하라는 플래
	//오규리 :: 질문 2) Disconnect에 대한 플래그는 언제 ON/OFF? - PQCS 안하면 disconnect에 대한 플래그 없
	//오규리 :: 질문 3) 원장님이 IO Count랑 동시에 체크하라는 플래그는 Disconnect(Delete라 표현하심) 플래그였다.
				//왜 동시에 체크해야 하는가? - 동시삭제할경우 같은 인덱스의 다른 세션이 삭제 될 수 있

	//오규리 :: 질문 4) 원장님이 말씀하신 Delete Flag는 지금 IO카운트 바깥에 있고, PQCS여부로 체크한다고 하였다.
				//원래의 DeleteFlag가 켜지는 조건은 무엇이었는가? - 원장님이 말한 delete 플래그와 내 코드상의 disconnect 플래그, release플래그는 다른존재임

	//오규리 :: 질문 5) 원래의 Delete Flag가 켜지는 조건 - 지울때, (IOcount = 0)
	//오규리 :: 질문 6) 현재 IO취소플래그가 켜지는 조건? - 컨텐츠쪽 코드에서 임의의 이유로 disconnectReq 할때, 또는 네트워크 코드에서도 가
	//오규리 :: 질문 7) 그러면 IOcount가 0이어서 플래그가 켜진게 아니고, 임의로 호출하는 타이밍에 켜진다?
				//이게 오작동 일으키지 않나요..??? 

	// 일단 제 스탠스 
	//		1. 최소한의 성능을 만족하자 : 5000개 넣었을때 1만 job TPS
	//					=> 컨텐츠 코드를 개판으로 짯거나, 네트워크 코드를 개판으로 짜서 '실제 경합' 으로 인해 발생할 문제를 재현하지 못할정도로 느린 코드라면
	//					=> 애초 발생 가능한 에러지만 발생하지 않을 수 있음, (코드가 느려서)
	//		2. 최소한의 안정성을 만족하자 : 3일 7일간 죽지 않는 서버 구현
	//					=> 당연한것
	//		3. 버그수정 (결함 수정)
	//		4. 버그수정 (에러 수정)


	//writeMessageBuffer("disconnect %d", info);
	//LOG(logLevel::Info, LO_TXT, "123");

	USHORT index = (USHORT)sessionPtr->ID;
	int sock = sessionPtr->socket;
	if(closesocket(sessionPtr->socket) != 0)
		LOG(logLevel::Info, LO_TXT, "close socket error " + to_string(sock) + " socket, error " + to_string(GetLastError()));
	//LOG(logLevel::Info, LO_TXT, "close " + to_string(sock) + " socket");
	sessionPtr->disconnectUnregist();
	//sessionArray[index] = nullptr;

	sessionPtr->bufferClear();
	//AcquireSRWLockExclusive(&sessionPoolLock);
	sessionPool.Free(sessionPtr);
	//ReleaseSRWLockExclusive(&sessionPoolLock);

	AcquireSRWLockExclusive(&indexStackLock);
	indexStack.push(index);
	ReleaseSRWLockExclusive(&indexStackLock);

	OnDisconnect(sessionPtr->ID);
	//LOG(logLevel::Info, LO_TXT, "release session " + to_string(sock) + " socket, " + to_string(index) + " index");
}

void Network::deleteSession(sessionPtr* sessionPtr)
{
	deleteSession(sessionPtr->ptr);
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

	//AcquireSRWLockExclusive(&sessionPoolLock);
	ret.first = sessionPool.GetCapacityCount();
	ret.second = sessionPool.GetUseCount();
	//ReleaseSRWLockExclusive(&sessionPoolLock);

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

sessionPtr Network::findSession(UINT64 sessionID)
{
	sessionPtr ret(sessionArray[(USHORT)sessionID], this);

	if (sessionArray[(USHORT)sessionID]->ID != sessionID)
		return sessionPtr(nullptr, this);

	return ret;
}

////////////////////////////////////////////////////////////////////////////////////
// 이하 컨텐츠단에서 먼저 호출 가능한 함수들
////////////////////////////////////////////////////////////////////////////////////
bool Network::sendPacket(UINT64 sessionID, packet& _pakcet)
{
	sessionPtr s = findSession(sessionID);

	if (!s.valid())
		return false;

	/*/
	{
		if (s.ptr->sendBuffer.freeSize() >= sizeof(serializer*))
		{
			_pakcet.buffer->incReferenceCounter();
			auto ret = s.ptr->sendBuffer.push((char*)&(_pakcet.buffer), sizeof(serializer*));


			if (!s.ptr->sendIO())
			{
				if (s.ptr->decrementIO() == 0) {
					deleteSession(s.ptr);
					return false;
				}
			}
		}
	}
	/*/
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
	//
	return true;
}

void Network::disconnectReq(UINT64 sessionID)
{
	sessionPtr s = findSession(sessionID);

	if (s.ptr == nullptr)
		return;

	s.ptr->disconnectRegist();
	if (CancelIoEx((HANDLE)s.ptr->socket, &(s.ptr->recvOverlapped)) == 0)
	{
		int errorCode = GetLastError();
		if (errorCode != ERROR_NOT_FOUND){
			LOG(logLevel::Error, LO_TXT, "disconnectReq recv CancelIoEx error" + to_string(errorCode));
		}
	}
	else{
		if (s.decrementIO() == 0)
			deleteSession(s.ptr);
	}

	if (CancelIoEx((HANDLE)s.ptr->socket, &(s.ptr->sendOverlapped)) == 0)
	{
		int errorCode = GetLastError();
		if (errorCode != ERROR_NOT_FOUND) {
			LOG(logLevel::Error, LO_TXT, "disconnectReq send CancelIoEx error" + to_string(errorCode));
		}
	}
	else {
		if (s.decrementIO() == 0)
			deleteSession(s.ptr);
	}

}