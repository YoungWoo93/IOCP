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
	/// true : 정상동작 상태
	/// false : 어떤 이유로든 정상 동작 불가능
	/// </returns>
	bool sendIO();

	/// <summary>
	/// send 완료 통지에서 호출, sendflag를 제거하고 sended 버퍼의 시리얼라이저를 반환함. 만약 sendBuffer에 값이 남아있다면 재차 send를 요청함 (sendFlag 변화 없이)
	/// </summary>
	/// <param name="transfer"> 전송 완료통지 크기, sended처리 후 0이 되어야함</param>
	/// <param name="sendPacketCount"> 전송 완료 패킷 갯수, sended 과정에서 확인함</param>
	/// <returns>
	/// true : 정상동작
	/// false : 비정상 동작 (transfer zero, sended buffer corrupted, sendbuffer corrupted, WSAsend error ...)
	/// </returns>
	int sended(DWORD& transfer);


	/// <summary>
	/// sendBuffer에 패킷을 넣을떄 호출
	/// </summary>
	/// <returns>
	/// true : 넣음
	/// false : 넣을수 없음
	/// </returns>
	bool collectSendPacket(packet& p);


	/// <summary>
	/// 
	/// </summary>
	/// <returns>
	/// true : 정상동작 상태
	/// false : 어떤 이유로든 정상 동작 불가능
	/// </returns>
	bool recvIO();

	/// <summary>
	/// recv 완료 통지에서 호출, recvBuffer를 밀고, 만약 에러처리를함
	/// </summary>
	/// <param name="transfer"> 완료통지 전달 크기</param>
	/// <returns>
	/// true : 정상동작
	/// false : 비정상 동작 (recvBuffer full, recvBuffer flow, transfer zero ...)
	/// </returns>
	bool recved(DWORD& transfer);

	/// <summary>
	/// recv 완료 통지에서 호출, recv 버퍼에서 헤더를까서 완성된 데이터가 있으면 매개변수로 전달
	/// </summary>
	/// <returns>
	/// true : 꺼낼게 있음
	/// false : 꺼낼게 없음 
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


