#include <WS2tcpip.h>
#include <map>

#ifdef _DEBUG
#pragma comment(lib, "RingBufferD")
#pragma comment(lib, "MemoryPoolD")
#pragma comment(lib, "SerializerD")
#pragma comment(lib, "MessageLoggerD")
#pragma comment(lib, "crashDumpD")
#pragma comment(lib, "IOCPD")

#else
#pragma comment(lib, "RingBuffer")
#pragma comment(lib, "MemoryPool")
#pragma comment(lib, "Serializer")
#pragma comment(lib, "MessageLogger")
#pragma comment(lib, "crashDump")
#pragma comment(lib, "IOCP")

#endif

#include "IOCP/IOCP/network.h"
#include "IOCP/IOCP/session.h"

#include "networkClient.h"



UINT64 GlobalCount = 1;

NetworkClient::NetworkClient()
{

}
NetworkClient::~NetworkClient()
{
	// stop 시키기
}

bool NetworkClient::start(const std::map<std::string, std::pair<std::string, USHORT>>& serverList)
{
	WSADATA wsa;
	
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		OnError(ERRORCODE_WSASTARTUP);
		return false;
	}

	{
		sessionCount = 0;
		maxSession = 100;
		workerThreadCount = 2;
		runningThreadCount = 2;
		for (int i = maxSession - 1; i >= 0; i--)
			indexStack.push(i);

		sessionArray = new session * [maxSession];
		acceptTPSArr = new unsigned int[1];
		recvMessageTPSArr = new unsigned int[workerThreadCount];
		sendMessageTPSArr = new unsigned int[workerThreadCount];

		//sessionPool.init(maxSession);
		isRun = true;

		IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, runningThreadCount);
		if (IOCP == NULL) {
			writeMessageBuffer("CreateIoCompletionPort, GetLastError %d", GetLastError());
			OnError(ERRORCODE_CREATEKERNELOBJECT, messageBuffer);

			return false;
		}
	}

	for (auto info : serverList)
	{
		std::string name = info.first;
		std::string host = info.second.first;
		USHORT port = info.second.second;

		SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (serverSocket == INVALID_SOCKET) {
			//writeMessageBuffer("serverSocket %s (%s : %d)", name.c_str(), host.c_str(), port);
			OnError(ERRORCODE_SOCKET, "serverSocket");
			return false;
		}

		SOCKADDR_IN serverAddr;
		memset(&serverAddr, 0, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(port);
		inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr);
		int ret = connect(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
		if (ret == SOCKET_ERROR) {
			writeMessageBuffer("%s (%s : %d) ... GetLastError %d", name.c_str(), host.c_str(), port, GetLastError());
			OnError(ERRORCODE_CONNECT, messageBuffer);
			return false;
		}

		{
			AcquireSRWLockExclusive(&(sessionPoolLock));
			session* sessionPtr = sessionPool.Alloc();
			ReleaseSRWLockExclusive(&(sessionPoolLock));

			AcquireSRWLockExclusive(&indexStackLock);
			USHORT index = indexStack.top();
			indexStack.pop();
			ReleaseSRWLockExclusive(&indexStackLock);


			UINT64 sessionID = MAKE_SESSION_ID(sessionCount++, index);
			sessionPtr->init(sessionID, serverSocket, serverAddr.sin_port);
			sessionArray[index] = sessionPtr;
			infoIDMap[name] = sessionID;
			IDInfoMap[sessionID] = name;

			CreateIoCompletionPort((HANDLE)(sessionPtr->getSocket()), IOCP, (ULONG_PTR)sessionPtr, NULL);

			if (!sessionPtr->recvIO())
			{
				if (sessionPtr->decrementIO() == 0)
					deleteSession(sessionPtr);
			}
			OnNewConnect(sessionID);
		}
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
}

void NetworkClient::stop()
{
	isRun = false;

	if (IOCP != nullptr)
	{
		for (int i = 0; i < threadIndexMap.size(); i++)
			PostQueuedCompletionStatus(IOCP, 0, (ULONG_PTR)nullptr, nullptr);
		CloseHandle(IOCP);
	}

	//sessionPool.clear();

	WSACleanup();
}

void NetworkClient::OnNewConnect(UINT64 sessionID)
{
	// do something
	printf("new connect %s (%lld)\n\n", IDInfoMap[sessionID].c_str(), sessionID);
}
void NetworkClient::OnDisconnect(UINT64 sessionID)
{
	// do something
}

bool NetworkClient::OnConnectionRequest(ULONG ip, USHORT port)
{
	return true;
}

void NetworkClient::OnRecv(UINT64 sessionID, packet& _packet)
{
	// do something
	short h;
	UINT64 v;

	_packet >> h >> v;

	packet p;
	(*p.buffer) << GlobalCount++;
	setHeader(p);

	auto ret = sendPacket(sessionID, p);
}

void NetworkClient::OnSend(UINT64 sessionID, int sendsize)
{
	// do something
}

void NetworkClient::OnError(int code, const char* msg)
{
	if (errorCodeComment.find(code) == errorCodeComment.end())
		code = ERRORCODE_NOTDEFINE;

	LOG(logLevel::Error, LO_TXT, errorCodeComment[code] + msg);

	return;
}



DWORD WINAPI NetworkClient::workerThread(LPVOID arg)
{
	NetworkClient* core = ((NetworkClient*)arg);
	SHORT threadIndex = core->threadIndexMap[GetCurrentThread()];

	while (core->isRun)
	{
		DWORD transfer;
		session* sessionPtr;
		OVERLAPPED* overlap;

		bool GQCSresult = GetQueuedCompletionStatus(core->IOCP, &transfer, (PULONG_PTR)&sessionPtr, &overlap, INFINITE);
		if (sessionPtr == nullptr) //PQCS 에 의한 종료
			break;

		overlapped* o = ((overlapped*)overlap);
		if (GQCSresult == false || transfer == 0)
		{
			if (sessionPtr->decrementIO() == 0)
				core->deleteSession(sessionPtr);

			continue;
		}

		if (sessionPtr->sendOverlappedCheck(overlap))	//send 완료 블록
		{
			core->OnSend(sessionPtr->ID, transfer);
			core->sendMessageTPSArr[threadIndex] += sessionPtr->sended(transfer);

			//sessionPtr->sendIO();
			while (InterlockedCompareExchange16((short*) &(sessionPtr->sendFlag), 0, 0) == 0 && !(sessionPtr->sendBuffer.empty()))
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
		else if (sessionPtr->sendOverlappedCheck(overlap))	//recv 완료 블록
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

	return 0;
}
