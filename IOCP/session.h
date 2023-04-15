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

	/// <summary>
	/// 해당 세션이 모종의 이유로 서버측에서 먼저 끊기 요청된 경우, 해당 요청 등록
	/// </summary>
	/// <returns>
	/// ioCount에 해당 플래그를 심은 뒤의 값
	/// </returns>
	UINT32 disconnectRegist();

	/// <summary>
	/// 해당 세션이 모종의 이유로 서버측에서 먼저 끊기 요청된 경우, 해당 요청수행 후 플래그 해제
	/// </summary>
	/// <returns>
	/// ioCount에 해당 플래그를 해제한 뒤의 값
	/// </returns>
	UINT32 disconnectUnregist();

	/// <summary>
	/// 해당 세션이 모종의 이유로 서버측에서 먼저 끊기 요청된 상태인지 체크
	/// </summary>
	/// <returns>
	/// true : 끊기 요청상태임
	/// false : 아님
	/// </returns>
	bool hasDisconnectRequest();

	SOCKET getSocket();
	UINT16 getPort();

	bool sendOverlappedCheck(overlapped* o);
	bool recvOverlappedCheck(overlapped* o);

	void bufferClear();

public:
	unsigned long long int ID;			//8		- 유저단에서 주로 사용

public:
	overlapped sendOverlapped;			//32	- send-recv 완료통지, send-recv IO 에서 사용
	overlapped recvOverlapped;			//32	- send-recv 완료통지, send-recv IO 에서 사용
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

