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

	UINT32 ID;

private:
	sessionPtr(session* _session, Network* _core);
	session* ptr;
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

	/// <summary>
	/// �ش� ������ ������ ������ ���������� ���� ���� ��û�� ���, �ش� ��û ���
	/// </summary>
	/// <returns>
	/// ioCount�� �ش� �÷��׸� ���� ���� ��
	/// </returns>
	UINT32 disconnectRegist();

	/// <summary>
	/// �ش� ������ ������ ������ ���������� ���� ���� ��û�� ���, �ش� ��û���� �� �÷��� ����
	/// </summary>
	/// <returns>
	/// ioCount�� �ش� �÷��׸� ������ ���� ��
	/// </returns>
	UINT32 disconnectUnregist();

	/// <summary>
	/// �ش� ������ ������ ������ ���������� ���� ���� ��û�� �������� üũ
	/// </summary>
	/// <returns>
	/// true : ���� ��û������
	/// false : �ƴ�
	/// </returns>
	bool hasDisconnectRequest();

	SOCKET getSocket();
	UINT16 getPort();

	bool sendOverlappedCheck(overlapped* o);
	bool recvOverlappedCheck(overlapped* o);

	void bufferClear();

public:
	unsigned long long int ID;			//8		- �����ܿ��� �ַ� ���

public:
	overlapped sendOverlapped;			//32	- send-recv �Ϸ�����, send-recv IO ���� ���
	overlapped recvOverlapped;			//32	- send-recv �Ϸ�����, send-recv IO ���� ���
	SOCKET socket;						//8
	UINT16 port;						//2

	UINT32 IOcount;						//4
	UINT32 sendFlag;					//4

	ringBuffer sendBuffer;				//56
	ringBuffer sendedBuffer;			//56
	ringBuffer recvBuffer;				//56

	UINT32 debugFlag;					//4
};
//	total 288

//0	 1	 2	 3	 4	 5	 6	 7	 8	 9	 10	 11	 12	 13	 14	 15	 16	 17	 18	 19	 20	 21	 22	 23	 24	 25	 26	 27	 28	 29	 30	 31	 32	 33	 34	 35	 36	 37	 38	 39	 40	 41	 42	 43	 44	 45	 46	 47	 48	 49	 50	 51	 52	 53	 54	 55	 56	 57	 58	 59	 60	 61	 62	 63
//	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	|	
//		ID	(0~7) 8/8			|	send overlapped	(8 ~ 47) 40/8																																|	recv overlapped (48 ~  87(13)) 40/8
//
//
//
//
//
//

