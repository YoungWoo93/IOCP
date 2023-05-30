#pragma comment(lib, "ws2_32")

#ifdef _DEBUG
#pragma comment(lib, "RingBufferD")
#pragma comment(lib, "MemoryPoolD")
#pragma comment(lib, "crashDumpD")

#else
#pragma comment(lib, "RingBuffer")
#pragma comment(lib, "MemoryPool")
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
//#include "MessageLogger/MessageLogger/MessageLogger.h"
//#include "crashDump/crashDump/crashDump.h"
//
#include "errorDefine.h"
#include "packet.h"
#include "network.h"
#include "monitoringTools/monitoringTools/messageLogger.h"
#include "monitoringTools/monitoringTools/performanceProfiler.h"
#include "monitoringTools/monitoringTools/resourceMonitor.h"
#pragma comment(lib, "monitoringTools\\x64\\Release\\monitoringTools")


/*/

sessionPtr::sessionPtr(session* _session, Network* _core) : ptr(_session), core(_core) {
	if (_session != nullptr)
	{
		if(_session->incrementIO() & 0x80000000 != 0){
			if (_session->decrementIO() == 0) {
				core->deleteSession(_session);
			}
		}

		ptr = nullptr;
	}
}
/*/
session::session()
	: ID(0), socket(0),
	IOcount(0), sendFlag(0), 
	sendBuffer(1000), sendedBuffer(4096), recvBuffer(4096) {

}
session::session(UINT64 id, SOCKET sock)
	: ID(id), socket(sock),
	IOcount(0), sendFlag(0),
	sendBuffer(1000), sendedBuffer(4096), recvBuffer(4096) {

}
void session::init(UINT64 id, SOCKET sock, UINT16 _port)
{
	ID = id;
	socket = sock;
	port = _port;

	incrementIO();
	
	sendFlag = 0;
	cancelIOFlag = 0;

	InterlockedAnd((LONG*)&IOcount, (LONG)0x7fffffff);
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



/*/
bool session::sendIO()
{
	if (hasCancelIOFlag())
		return true;

	if (InterlockedExchange16(&sendFlag, 1) == 1)
		return true;

	size_t sendBufferSize = sendBuffer.size();	// 잠재적 에러 가능, size는 1 이상이지만 연결은 안된 상태일데
	if (sendBufferSize == 0) { // 보낼게 없어서 종료, 동작은 정상상황
		InterlockedExchange16(&sendFlag, 0);
		return true;
	}

	incrementIO();

	size_t packetCount = sendBufferSize;
	size_t WSAbufferCount = 0;
	WSABUF buffer[100];

	// size 체크를 굳이 할 필요가 없을 가능성이 높음
	for (WSAbufferCount = 0; WSAbufferCount < packetCount; WSAbufferCount++)
	{
		serializer* temp = 0;
		if (sendBuffer.pop(&temp) == false)
		{
			WSAbufferCount--;
			sendBufferSize--;
			continue;
		}

		buffer[WSAbufferCount].buf = temp->getHeadPtr();
		buffer[WSAbufferCount].len = (ULONG)temp->size();

		sendedBuffer.push((char*)&temp, sizeof(serializer*));
	}

	DWORD temp1 = 0;

	memset(&sendOverlapped, 0, sizeof(OVERLAPPED));
	auto ret = WSASend(socket, buffer, (DWORD)WSAbufferCount, &temp1, 0, (LPWSAOVERLAPPED)&sendOverlapped, NULL);
	

	if (ret == SOCKET_ERROR) {
		auto errorCode = GetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			if (errorCode != 10054 && errorCode != 10053)
			{
				LOG(logLevel::Error, LO_TXT, "WSASend Error " + to_string(errorCode) + "\tby socket " + to_string(socket) + ", id " + to_string(ID));
				InterlockedExchange16(&sendFlag, 0);
				//int* temp = nullptr;
				//*temp = 10;
			}
			return false;
		}
	}

	if (hasCancelIOFlag()){
		CancelIoEx((HANDLE)socket, &sendOverlapped);
	}

	return true;
}
/*/

void session::sendIO()
{
	if (hasCancelIOFlag())
		return;

	if (InterlockedExchange16(&sendFlag, 1) == 1)
		return;

	size_t sendBufferSize = sendBuffer.size();	// 잠재적 에러 가능, size는 1 이상이지만 연결은 안된 상태일데
	if (sendBufferSize == 0) { // 보낼게 없어서 종료, 동작은 정상상황
		InterlockedExchange16(&sendFlag, 0);
		return;
	}

	incrementIO();

	size_t packetCount = sendBufferSize;
	size_t WSAbufferCount = 0;
	WSABUF buffer[100];

	for (WSAbufferCount = 0; WSAbufferCount < packetCount; WSAbufferCount++)
	{
		serializer* temp = 0;
		if (sendBuffer.pop(&temp) == false)
		{
			WSAbufferCount--;
			sendBufferSize--;
			continue;
		}

		buffer[WSAbufferCount].buf = temp->getHeadPtr();
		buffer[WSAbufferCount].len = (ULONG)temp->size();

		sendedBuffer.push((char*)&temp, sizeof(serializer*));
	}

	DWORD temp1 = 0;

	memset(&sendOverlapped, 0, sizeof(OVERLAPPED));
	auto ret = WSASend(socket, buffer, (DWORD)WSAbufferCount, &temp1, 0, (LPWSAOVERLAPPED)&sendOverlapped, NULL);

	if (ret == SOCKET_ERROR) {
		auto errorCode = GetLastError();
		if (errorCode != WSA_IO_PENDING) // send는 지금 IO_PENDING을 타지 않는다.
		{
			if (errorCode != 10054 && errorCode != 10053)
			{
				LOG(logLevel::Error, LO_TXT, "WSASend Error " + to_string(errorCode) + "\tby socket " + to_string(socket) + ", id " + to_string(ID));
				InterlockedExchange16(&sendFlag, 0);
				//int* temp = nullptr;
				//*temp = 10;
			}

			cancelIORegist();

			if (CancelIoEx((HANDLE)socket, &(recvOverlapped)) == 0)
			{
				int errorCode = GetLastError();
				if (errorCode != ERROR_NOT_FOUND) {
					LOGOUT_EX(logLevel::Error, LO_TXT, "lib") << "disconnectReq recv CancelIoEx error " << errorCode << " index " << (USHORT)this->ID << LOGEND;
				}
			}

			if (decrementIO() == 0)
				core->deleteSession(this); // [질문1] 이부분, 각 세션에서 네트워크 엔진의 정보를 알고있게 했는가?

			return;
		}

		if (hasCancelIOFlag()) 
		{
			if (CancelIoEx((HANDLE)socket, &sendOverlapped) == 0)
			{
				int errorCode = GetLastError();
				if (errorCode != ERROR_NOT_FOUND) {
					LOGOUT_EX(logLevel::Error, LO_TXT, "lib") << "disconnectReq send CancelIoEx error " << errorCode << " index " << (USHORT)this->ID << LOGEND;
				}
			}
		}
	}

	return;
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

		int temp = packetBuffer->decReferenceCounter();
		if (temp == 0)
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
bool session::collectSendPacket(packet p)
{

	if (sendBuffer.push(p.buffer))
		return true;
	else
		return false;
}

bool session::collectSendPacket(serializer* p)
{
	if (sendBuffer.push(p))
		return true;
	else
		return false;
}

/// <summary>
/// 
/// </summary>
/// <returns>
/// true : 정상동작 상태
/// false : 어떤 이유로든 정상 동작 불가능
/// </returns>
/// 
/*/ [질문1] 연관
bool session::recvIO()
{
	if (hasCancelIOFlag())
		return true;

	WSABUF buffer[2];
	int bufferCount = 1;
	size_t freeSize = recvBuffer.freeSize();

	buffer[0].buf = recvBuffer.tail();
	buffer[0].len = (ULONG)recvBuffer.DirectEnqueueSize();
	freeSize -= (ULONG)recvBuffer.DirectEnqueueSize();

	if (freeSize > 0)
	{
		buffer[1].buf = recvBuffer.bufferPtr();
		buffer[1].len = freeSize;
		bufferCount = 2;
	}


	DWORD temp1 = 0;
	DWORD temp2 = 0;

	memset(&recvOverlapped, 0, sizeof(OVERLAPPED));
	auto ret = WSARecv(socket, buffer, bufferCount, &temp1, &temp2, (LPWSAOVERLAPPED)&recvOverlapped, NULL);

	if (ret == SOCKET_ERROR) {
		auto errorCode = GetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			if (errorCode != 10054 && errorCode != 10053)
			{
				LOG(logLevel::Error, LO_TXT, "WSARecv Error " + to_string(errorCode) + "\tby socket " + to_string(socket) + ", id " + to_string(ID));
				//int* temp = nullptr;
				//*temp = 10;
			}
			
			return false;
		}
	}
	if (hasCancelIOFlag())
	{
		CancelIoEx((HANDLE)socket, &sendOverlapped);
	}
	return true;
}
/*/

void session::recvIO()
{
	if (hasCancelIOFlag())
		return;

	WSABUF buffer[2];
	int bufferCount = 1;
	size_t freeSize = recvBuffer.freeSize();

	buffer[0].buf = recvBuffer.tail();
	buffer[0].len = (ULONG)recvBuffer.DirectEnqueueSize();
	freeSize -= (ULONG)recvBuffer.DirectEnqueueSize();

	if (freeSize > 0)
	{
		buffer[1].buf = recvBuffer.bufferPtr();
		buffer[1].len = freeSize;
		bufferCount = 2;
	}

	DWORD temp1 = 0;
	DWORD temp2 = 0;

	incrementIO();

	memset(&recvOverlapped, 0, sizeof(OVERLAPPED));
	auto ret = WSARecv(socket, buffer, bufferCount, &temp1, &temp2, (LPWSAOVERLAPPED)&recvOverlapped, NULL);

	if (ret == SOCKET_ERROR) {
		auto errorCode = GetLastError(); // recv는 지금 IO_PENDING을 탄다

		if (errorCode != WSA_IO_PENDING)
		{
			if (errorCode != 10054 && errorCode != 10053)
			{
				LOG(logLevel::Error, LO_TXT, "WSARecv Error " + to_string(errorCode) + "\tby socket " + to_string(socket) + ", id " + to_string(ID));
			}

			cancelIORegist();

			if (CancelIoEx((HANDLE)socket, &sendOverlapped) == 0)
			{
				int errorCode = GetLastError();
				if (errorCode != ERROR_NOT_FOUND) {
					LOGOUT_EX(logLevel::Error, LO_TXT, "lib") << "disconnectReq recv CancelIoEx error " << errorCode << " index " << (USHORT)this->ID << LOGEND;
				}
			}

			if (decrementIO() == 0)
				core->deleteSession(this);

			return;
		}

		if (hasCancelIOFlag())
		{
			if (CancelIoEx((HANDLE)socket, &recvOverlapped) == 0)
			{
				int errorCode = GetLastError();
				if (errorCode != ERROR_NOT_FOUND) {
					LOGOUT_EX(logLevel::Error, LO_TXT, "lib") << "disconnectReq recv CancelIoEx error " << errorCode << " index " << (USHORT)this->ID << LOGEND;
				}
			}
		}
	}

	return;
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
bool session::recvedPacket(serializer* p)
{
	if (recvBuffer.size() >= sizeof(packet::packetHeader))
	{
		packet::packetHeader h;
		int size = recvBuffer.front((char*)&h, sizeof(packet::packetHeader));
		if (size != sizeof(packet::packetHeader))
			return false;

		if (recvBuffer.size() >= sizeof(packet::packetHeader) + h.size)
		{
			recvBuffer.pop((char*)p->getBufferPtr(), sizeof(packet::packetHeader) + h.size);
			p->moveRear(h.size);
			p->moveFront(-(int)sizeof(packet::packetHeader));

			return true;
		}
		else
			return false;
	}

	return false;
}

/*/
bool session::recvedPacket(serializer* p)
{
	if (recvBuffer.size() >= sizeof(packet::packetHeader))
	{
		packet::packetHeader h;
		int size = recvBuffer.front((char*)&h, sizeof(packet::packetHeader));
		if (size != sizeof(packet::packetHeader))
			return false;

		if (recvBuffer.size() >= sizeof(packet::packetHeader) + h.size)
		{
			recvBuffer.pop((char*)p.getPacketHeader(), sizeof(packet::packetHeader) + h.size);
			p.buffer->moveRear(h.size);
			p.buffer->moveFront(-(int)sizeof(packet::packetHeader));

			return true;
		}
		else
			return false;
	}

	return false;
}
/*/



/// <summary>
/// 해당 세션이 모종의 이유로 서버측에서 먼저 끊기 요청된 경우, 해당 플래그 설정
/// </summary>
/// <returns>
/// ioCount에 해당 플래그를 해제한 뒤의 값
/// </returns>
UINT32 session::cancelIORegist()
{
	return cancelIOFlag = 1;
}

/// <summary>
/// 해당 세션이 모종의 이유로 서버측에서 먼저 끊기 요청된 경우, 해당 요청수행 후 플래그 해제
/// </summary>
/// <returns>
/// ioCount에 해당 플래그를 해제한 뒤의 값
/// </returns>
UINT32 session::cancelIOUnregist()
{
	return cancelIOFlag = 0;
}

/// <summary>
/// 해당 세션이 모종의 이유로 서버측에서 먼저 끊기 요청된 상태인지 체크
/// </summary>
/// <returns>
/// true : 끊기 요청상태임
/// false : 아님
/// </returns>
bool session::hasCancelIOFlag()
{
	return cancelIOFlag == 1;
}

SOCKET session::getSocket() {
	return socket;
}
UINT16 session::getPort() {
	return port;
}

bool session::sendOverlappedCheck(OVERLAPPED* o) {
	return o == &sendOverlapped;
}
bool session::recvOverlappedCheck(OVERLAPPED* o) {
	return o == &recvOverlapped;
}

void session::bufferClear()
{
	serializer* packetBuffer;

	while (sendBuffer.pop(&packetBuffer))
	{
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
