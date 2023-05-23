#pragma once

#include <string.h>
#define __VER__ (strrchr(__FILE__, '_') ? strrchr(__FILE__, '_') + 1 : __FILE__)
#define _USE_LOGGER_

//	v.5.1
// 
//	5.0 + disconnect�� deleteSession�� ���� findSession���� ioCount�� referenceCountó�� ����� 
//	
//

#pragma comment(lib, "ws2_32")

#include <WinSock2.h>
#include <iostream>
#include <stack>
#include <map>

#include "RingBuffer/RingBuffer/RingBuffer.h"
#include "MemoryPool/MemoryPool/MemoryPool.h"
#include "Serializer/Serializer/Serializer.h"

#include "errorDefine.h"
#include "packet.h"

#define writeMessageBuffer(...)					sprintf_s(messageBuffer, 256, __VA_ARGS__);
#define session_buffer_size						1024*4
#define	GET_INDEX(sessionID)					(unsigned short)((unsigned long long)sessionID)
#define	MAKE_SESSION_ID(sessionCount, index)	((unsigned long long)index | ((unsigned long long)sessionCount << 48) )


class sessionPtr;
class session;

class Network {
	friend class sessionPtr;
public:
	Network();
	~Network();

	bool start(const USHORT port, const UINT16 maxSessionSize,
		const UINT8 workerThreadSize = 4, const UINT8 runningThreadSize = 4,
		const int backLogSize = 1000, const ULONG host = INADDR_ANY);

	void stop();

	static DWORD WINAPI networkThread(LPVOID arg);
	static DWORD WINAPI acceptThread(LPVOID arg);
	static DWORD WINAPI workerThread(LPVOID arg);

	size_t getSessionCount();
	std::pair<size_t, size_t> getSessionPoolMemory();
	unsigned int getAcceptTPS();
	unsigned int getRecvMessageTPS();
	unsigned int getSendMessageTPS();

protected:
	void deleteSession(session* sessionPtr);
	void deleteSession(sessionPtr* sessionPtr);

	virtual void OnNewConnect(UINT64 sessionID) = 0;
	virtual void OnDisconnect(UINT64 sessionID) = 0;
	virtual bool OnConnectionRequest(ULONG ip, USHORT port) = 0;

	virtual void OnRecv(UINT64 sessionID, packet& _packet) = 0;	// < ��Ŷ ���� �Ϸ� ��
	virtual void OnSend(UINT64 sessionID, int sendsize) = 0;        // < ��Ŷ �۽� �Ϸ� ��

	/// <summary>
	/// �̷�������� OnInfo, OnLogging (?) �� �����ҵ�? �´����� ���� �Ǵ��ʿ�
	/// 
	/// </summary>
	/// <param name="code">ERRORCODE_XXX ��ũ�θ� �̿��Ұ�</param>
	/// <param name="msg">
	/// ���� ���� ��Ʈ���� �ʿ��ϴٸ�
	/// writeMessageBuffer("�� ��ũ�θ� �̿��� ��, �ִ� �޽��� ũ��� %d ��", 256); 
	/// </param>
	virtual void OnError(int errorcode, const char* msg = "") = 0;

public:
	////////////////////////////////////////////////////////////////////////////////////
	// ���� �������ܿ��� ���� ȣ�� ������ �Լ���
	////////////////////////////////////////////////////////////////////////////////////
	bool sendPacket(UINT64 sessionID, packet& _pakcet);
	void disconnectReq(UINT64 sessionID);
	sessionPtr findSession(UINT64 sessionID);

protected:
	UINT8 runningThreadCount;
	UINT8 workerThreadCount;
	unsigned int* acceptTPSArr;
	unsigned int* recvMessageTPSArr;
	unsigned int* sendMessageTPSArr;
	unsigned int acceptTPS;
	unsigned int recvMessageTPS; 
	unsigned int sendMessageTPS;

	SRWLOCK sessionPoolLock;
	ObjectPool<session> sessionPool;
public:
	session** sessionArray;
	//���Ը� :: ���࿡ ���� �ȳ����� ����ü �迭�� ���°͵� ����غ���!!!!!


protected:
	SRWLOCK indexStackLock;
	std::stack<UINT16> indexStack;

	SOCKET listen_socket;
	HANDLE IOCP;
	HANDLE hAcceptThread;
	HANDLE hNetworkThread;
	std::map<HANDLE, USHORT> threadIndexMap;

	UINT32 sessionCount;
	int maxSession;
	bool isRun;

	char messageBuffer[256];
};