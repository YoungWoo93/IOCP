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

#include "session.h"

//#include <iostream>
//
//#include <WinSock2.h>
//#include <stack>
//#include <unordered_map>
//
//#include "RingBuffer/RingBuffer/RingBuffer.h"
//#include "MemoryPool/MemoryPool/MemoryPool.h"
#include "Serializer/Serializer/Serializer.h"
//#include "MessageLogger/MessageLogger/MessageLogger.h"
//#include "crashDump/crashDump/crashDump.h"
//
#include "errorDefine.h"
#include "packet.h"
#include "network.h"



overlapped::overlapped(session* s) : sessionptr(s) {
	memset(&overlap, 0, sizeof(OVERLAPPED));
}
void overlapped::init(session* s) {
	sessionptr = s;
	memset(&overlap, 0, sizeof(OVERLAPPED));
}




sessionPtr::~sessionPtr() {
	if (ptr == nullptr)
		return;

	// 이곳에서 참조카운트를 감소시키고
	// 만약 참조카운트 (= ioCount) 0 인상황 (delete 되어야 하는데 이 구조체의 참조로 인하여 삭제되지 못한경우였음) 이면
	// 여기서 deleteSession의 루틴을 타주어야 한다.
	if (ptr->decrementIO() == 0)
		core->deleteSession(ptr);

}

sessionPtr::sessionPtr(const sessionPtr& s) {
	s.ptr->incrementIO();
	ptr = s.ptr;
	core = s.core;
}

sessionPtr::sessionPtr(session* _session, Network* _core) : ptr(_session), core(_core) {
	if (_session != nullptr)
	{
		if (_session->incrementIO() == 0)
		{
			core->deleteSession(ptr);
			ptr = nullptr;
		}
	}
}

sessionPtr& sessionPtr::operator= (const sessionPtr& s) {
	if (s.ptr->incrementIO() == 1)
	{
		core->deleteSession(ptr);
		ptr = nullptr;
	}
	else
		ptr = s.ptr;
	core = s.core;
}







session::session()
	: sendOverlapped(this), recvOverlapped(this), ID(0), socket(0),
	IOcount(1), sendFlag(0),
	sendBuffer(4096), sendedBuffer(4096), recvBuffer(4096) {
	onConnect = 0;
}
session::session(UINT64 id, SOCKET sock)
	: sendOverlapped(this), recvOverlapped(this), ID(id), socket(sock),
	IOcount(1), sendFlag(0),
	sendBuffer(4096), sendedBuffer(4096), recvBuffer(4096) {
	onConnect = 0;
}
void session::init(UINT64 id, SOCKET sock, UINT16 _port)
{
	ID = id;
	socket = sock;
	port = _port;

	IOcount = 1;
	sendFlag = 0;
	bufferClear();
}

UINT32 session::decrementIO() {
	return InterlockedDecrement(&IOcount);
}
UINT32 session::incrementIO() {
	return InterlockedIncrement(&IOcount);
}

/// <summary>
/// 
/// </summary>
/// <returns>
/// true : 정상동작 상태
/// false : 어떤 이유로든 정상 동작 불가능
/// </returns>

#include "Profiler/Profiler/Profiler.h"
#ifdef _DEBUG
#pragma comment(lib, "ProfilerD")

#else
#pragma comment(lib, "Profiler")
#endif


bool session::sendIO()
{
	if (InterlockedExchange(&sendFlag, 1) == 1)
		return true;

	size_t sendBufferSize = sendBuffer.size();
	if (sendBufferSize == 0) { // 보낼게 없어서 종료, 동작은 정상상황
		InterlockedExchange(&sendFlag, 0);
		return true;
	}

	if ((sendBufferSize % sizeof(serializer*)) != 0) {// sendBuffer가 꼬인상태, 동작 비정상으로 종료되어야 함
		LOG(logLevel::Error, LO_TXT, "sendBuffer 꼬임");
		InterlockedExchange(&sendFlag, 0);
		return false;
	}

	InterlockedIncrement(&IOcount);

	size_t packetCount = min(sendBufferSize / sizeof(serializer*), 100);
	size_t WSAbufferCount = 0;
	WSABUF buffer[100];
	memset(buffer, 0, sizeof(WSABUF) * packetCount);

	for (WSAbufferCount = 0; WSAbufferCount < packetCount; WSAbufferCount++)
	{
		serializer* temp;
		sendBuffer.pop((char*)&temp, sizeof(serializer*));
		buffer[WSAbufferCount].buf = temp->getHeadPtr();
		buffer[WSAbufferCount].len = (ULONG)temp->size();
		sendedBuffer.push((char*)&temp, sizeof(serializer*));
	}

	DWORD temp1 = 0;


	auto ret = WSASend(socket, buffer, (DWORD)WSAbufferCount, &temp1, 0, (LPWSAOVERLAPPED)&sendOverlapped, NULL);


	if (ret == SOCKET_ERROR) {
		auto errorCode = GetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			if (errorCode != 10054)
			{
				LOG(logLevel::Error, LO_TXT, "WSASend Error " + to_string(errorCode) + "\tby socket " + to_string(socket) + ", id " + to_string(ID));
				InterlockedExchange(&sendFlag, 0);
				InterlockedDecrement(&IOcount);
			}
			return false;
		}
	}

	return true;
}

/// <summary>
/// send 완료 통지에서 호출,sended 버퍼의 시리얼라이저를 반환함. (sendFlag 변화 없이)
/// </summary>
/// <param name="transfer"> 전송 완료통지 크기, sended처리 후 0이 되어야함</param>
/// <param name="sendPacketCount"> 전송 완료 패킷 갯수, sended 과정에서 확인함</param>
/// <returns>
/// true : 정상동작
/// false : 비정상 동작 (transfer zero, sended buffer corrupted, sendbuffer corrupted, WSAsend error ...)
/// </returns>
int session::sended(DWORD& transfer)
{
	DWORD temp = transfer;

	if (transfer <= 0)
		return -1;

	int sendedCount = 0;
	serializer* packetBuffer;

	while (transfer > 0)
	{
		int size = sendedBuffer.pop((char*)&packetBuffer, sizeof(serializer*));

		transfer -= (DWORD)packetBuffer->size();
		sendedCount++;

		if (packetBuffer->decReferenceCounter() == 0)
			serializerFree(packetBuffer);
	}

	if (transfer < 0)
		return -1;

	return sendedCount;
}


/// <summary>
/// sendBuffer에 패킷을 넣을떄 호출
/// </summary>
/// <returns>
/// true : 넣음
/// false : 넣을수 없음
/// </returns>
bool session::collectSendPacket(packet& p)
{
	if (sendBuffer.freeSize() >= sizeof(serializer*))
	{
		p.buffer->incReferenceCounter();
		auto ret = sendBuffer.push((char*)&(p.buffer), sizeof(serializer*));

		if (ret != sizeof(serializer*))
		{
			printf("brake here");
		}
		return true;
	}
	else {
		return false;
	}
}


/// <summary>
/// 
/// </summary>
/// <returns>
/// true : 정상동작 상태
/// false : 어떤 이유로든 정상 동작 불가능
/// </returns>
bool session::recvIO()
{
	WSABUF buffer;
	buffer.buf = recvBuffer.tail();
	buffer.len = (ULONG)recvBuffer.DirectEnqueueSize();

	DWORD temp1 = 0;
	DWORD temp2 = 0;
	auto ret = WSARecv(socket, &buffer, 1, &temp1, &temp2, (LPWSAOVERLAPPED)&recvOverlapped, NULL);

	if (ret == SOCKET_ERROR) {
		auto errorCode = GetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			if (errorCode != 10054)
				LOG(logLevel::Error, LO_TXT, "WSARecv Error " + to_string(errorCode) + "\tby socket " + to_string(socket) + ", id " + to_string(ID));

			return false;
		}
	}

	return true;
}

/// <summary>
/// recv 완료 통지에서 호출, recvBuffer를 밀고, 만약 에러처리를함
/// </summary>
/// <param name="transfer"> 완료통지 전달 크기</param>
/// <returns>
/// true : 정상동작
/// false : 비정상 동작 (recvBuffer full, recvBuffer flow, transfer zero ...)
/// </returns>
bool session::recved(DWORD& transfer)
{
	if (recvBuffer.freeSize() <= transfer || transfer == 0)
		return false;

	recvBuffer.MoveRear(transfer);

	return true;
}

/// <summary>
/// recv 완료 통지에서 호출, recv 버퍼에서 헤더를까서 완성된 데이터가 있으면 매개변수로 전달
/// </summary>
/// <returns>
/// true : 꺼낼게 있음
/// false : 꺼낼게 없음 
/// </returns>
bool session::recvedPacket(packet& p)
{
	if (recvBuffer.size() >= sizeof(packetHeader))
	{
		packetHeader h;
		recvBuffer.front((char*)&h, sizeof(packetHeader));

		if (recvBuffer.size() >= sizeof(packetHeader) + h.size)
		{
			p.buffer->moveRear(sizeof(packetHeader) + h.size);
			recvBuffer.pop(p.buffer->getHeadPtr(), sizeof(packetHeader) + h.size);

			return true;
		}
		else
			return false;
	}

	return false;
}

SOCKET session::getSocket() {
	return socket;
}
UINT16 session::getPort() {
	return port;
}

bool session::sendOverlappedCheck(overlapped* o) {
	return o == &sendOverlapped;
}
bool session::recvOverlappedCheck(overlapped* o) {
	return o == &recvOverlapped;
}

void session::bufferClear()
{
	serializer* packetBuffer;

	while (!sendBuffer.empty())
	{
		sendBuffer.pop((char*)&packetBuffer, sizeof(serializer*));
		if (packetBuffer->decReferenceCounter() == 0)
			serializerFree(packetBuffer);
	}

	while (!sendedBuffer.empty())
	{
		sendedBuffer.pop((char*)&packetBuffer, sizeof(serializer*));
		if (packetBuffer->decReferenceCounter() == 0)
			serializerFree(packetBuffer);
	}

	recvBuffer.clear();
}
