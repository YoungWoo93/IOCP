#pragma once


#pragma comment(lib, "ws2_32")

#include <WinSock2.h>
#include <stack>
#include <unordered_map>

#include "RingBuffer/RingBuffer/RingBuffer.h"
#include "MemoryPool/MemoryPool/MemoryPool.h"
#include "MessageLogger/MessageLogger/MessageLogger.h"

#include "errorDefine.h"
#include "packet.h"

struct overlapped;
class sessionPtr;
class session;
class Network;

struct overlapped{
public:
	overlapped(session* s);
	void init(session* s);

	OVERLAPPED overlap;
	session* sessionptr;
};



class sessionPtr {
	friend class Network;
public:
	~sessionPtr();

	sessionPtr(const sessionPtr& s);

	sessionPtr& operator= (const sessionPtr& s);
	session* ptr;

private:
	sessionPtr(session* _session, Network* _core);
	Network* core;
};



class session {
public:
	session();
	session(UINT64 id, SOCKET sock);
	void init(UINT64 id, SOCKET sock, UINT16 _port);

	UINT32 decrementIO();
	UINT32 incrementIO();

	/// <summary>
	/// 
	/// </summary>
	/// <returns>
	/// true : ������ ����
	/// false : � �����ε� ���� ���� �Ұ���
	/// </returns>
	bool sendIO();

	/// <summary>
	/// send �Ϸ� �������� ȣ��, sendflag�� �����ϰ� sended ������ �ø���������� ��ȯ��. ���� sendBuffer�� ���� �����ִٸ� ���� send�� ��û�� (sendFlag ��ȭ ����)
	/// </summary>
	/// <param name="transfer"> ���� �Ϸ����� ũ��, sendedó�� �� 0�� �Ǿ����</param>
	/// <param name="sendPacketCount"> ���� �Ϸ� ��Ŷ ����, sended �������� Ȯ����</param>
	/// <returns>
	/// true : ������
	/// false : ������ ���� (transfer zero, sended buffer corrupted, sendbuffer corrupted, WSAsend error ...)
	/// </returns>
	int sended(DWORD& transfer);


	/// <summary>
	/// sendBuffer�� ��Ŷ�� ������ ȣ��
	/// </summary>
	/// <returns>
	/// true : ����
	/// false : ������ ����
	/// </returns>
	bool collectSendPacket(packet& p);


	/// <summary>
	/// 
	/// </summary>
	/// <returns>
	/// true : ������ ����
	/// false : � �����ε� ���� ���� �Ұ���
	/// </returns>
	bool recvIO();

	/// <summary>
	/// recv �Ϸ� �������� ȣ��, recvBuffer�� �а�, ���� ����ó������
	/// </summary>
	/// <param name="transfer"> �Ϸ����� ���� ũ��</param>
	/// <returns>
	/// true : ������
	/// false : ������ ���� (recvBuffer full, recvBuffer flow, transfer zero ...)
	/// </returns>
	bool recved(DWORD& transfer);

	/// <summary>
	/// recv �Ϸ� �������� ȣ��, recv ���ۿ��� ������ �ϼ��� �����Ͱ� ������ �Ű������� ����
	/// </summary>
	/// <returns>
	/// true : ������ ����
	/// false : ������ ���� 
	/// </returns>
	bool recvedPacket(packet& p);
	SOCKET getSocket();
	UINT16 getPort();

	bool sendOverlappedCheck(overlapped* o);
	bool recvOverlappedCheck(overlapped* o);

	void bufferClear();

public:
	unsigned long long int ID;

public:
	overlapped sendOverlapped;
	overlapped recvOverlapped;
	SOCKET socket;
	UINT16 port;

	UINT32 IOcount;
	UINT32 sendFlag;
	bool disconnectFlag;

	ringBuffer sendBuffer;
	ringBuffer sendedBuffer;
	ringBuffer recvBuffer;

	UINT32 onConnect;
};


