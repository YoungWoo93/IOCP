#pragma comment(lib, "ws2_32")

#ifdef _DEBUG
#pragma comment(lib, "RingBufferD")
#pragma comment(lib, "MemoryPoolD")
#pragma comment(lib, "MessageLoggerD")
#pragma comment(lib, "crashDumpD")

#else
#pragma comment(lib, "RingBuffer")
#pragma comment(lib, "MemoryPool")
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
//#include "MessageLogger/MessageLogger/MessageLogger.h"
//#include "crashDump/crashDump/crashDump.h"
//
#include "errorDefine.h"
#include "packet.h"
#include "network.h"


sessionPtr::~sessionPtr() {
	if (ptr == nullptr)
		return;

	// �̰����� ����ī��Ʈ�� ���ҽ�Ű��
	// ���� ����ī��Ʈ (= ioCount) 0 �λ�Ȳ (delete �Ǿ�� �ϴµ� �� ����ü�� ������ ���Ͽ� �������� ���Ѱ�쿴��) �̸�
	// ���⼭ deleteSession�� ��ƾ�� Ÿ�־�� �Ѵ�.
	if (ptr->decrementIO() == 0)
		core->deleteSession(ptr);

}

sessionPtr::sessionPtr(const sessionPtr& s) {
	if (s.ptr != nullptr)
		s.ptr->incrementIO();
	ptr = s.ptr;
	core = s.core;
}

sessionPtr::sessionPtr(session* _session, Network* _core) : ptr(_session), core(_core) {
	if (_session != nullptr)
	{
		if (_session->incrementIO() == 1)
		{
			_session->decrementIO();
			ptr = nullptr;
		}
	}
}

session::session()
	: ID(0), socket(0),
	IOcount(1), sendFlag(0),
	sendBuffer(1000), sendedBuffer(4096), recvBuffer(4096) {
}
session::session(UINT64 id, SOCKET sock)
	: ID(id), socket(sock),
	IOcount(1), sendFlag(0),
	sendBuffer(1000), sendedBuffer(4096), recvBuffer(4096) {
}
void session::init(UINT64 id, SOCKET sock, UINT16 _port)
{
	ID = id;
	socket = sock;
	port = _port;

	IOcount = 1;
	sendFlag = 0;
	bufferClear();

	memset(&sendOverlapped, 0, sizeof(OVERLAPPED));
	memset(&recvOverlapped, 0, sizeof(OVERLAPPED));
}

UINT32 session::decrementIO() {
	return InterlockedDecrement(&IOcount) & 0x7fffffff;
}
UINT32 session::incrementIO() {
	return InterlockedIncrement(&IOcount) & 0x7fffffff;
}

/// <summary>
/// 
/// </summary>
/// <returns>
/// true : ������ ����
/// false : � �����ε� ���� ���� �Ұ���
/// </returns>

#include "Profiler/Profiler/Profiler.h"
#ifdef _DEBUG
#pragma comment(lib, "ProfilerD")

#else
#pragma comment(lib, "Profiler")
#endif


bool session::sendIO()
{
	if (hasDisconnectRequest())
		return true;

	if (InterlockedExchange16(&sendFlag, 1) == 1)
		return true;

	size_t sendBufferSize = sendBuffer.size();	// ������ ���� ����, size�� 1 �̻������� ������ �ȵ� �����ϵ�
	if (sendBufferSize == 0) { // ������ ��� ����, ������ �����Ȳ
		InterlockedExchange16(&sendFlag, 0);
		return true;
	}

	InterlockedIncrement(&IOcount);

	//if ((sendBufferSize % sizeof(serializer*)) != 0) {// sendBuffer�� ���λ���, ���� ���������� ����Ǿ�� ��
	//	LOG(logLevel::Error, LO_TXT, "sendBuffer ����");
	//	InterlockedExchange16(&sendFlag, 0);
	//	return false;
	//}


	//size_t packetCount = min(sendBufferSize / sizeof(serializer*), 100);
	size_t packetCount = sendBufferSize;
	size_t WSAbufferCount = 0;
	WSABUF buffer[100]; 
	memset(buffer, 0, sizeof(WSABUF) * packetCount);

	for (WSAbufferCount = 0; WSAbufferCount < packetCount; WSAbufferCount++)
	{
		serializer* temp = 0;
		////1sendBuffer.pop((char*)&temp, sizeof(serializer*));
		////1sendBuffer.pop(temp);

		buffer[WSAbufferCount].buf = temp->getHeadPtr();
		buffer[WSAbufferCount].len = (ULONG)temp->size();
		sendedBuffer.push((char*)&temp, sizeof(serializer*));
		//���Ը� :: WSABUF ��� ��õ! �� ������ ���¥������� ��õ...
	}

	DWORD temp1 = 0;


	auto ret = WSASend(socket, buffer, (DWORD)WSAbufferCount, &temp1, 0, (LPWSAOVERLAPPED)&sendOverlapped, NULL);
	

	if (ret == SOCKET_ERROR) {
		auto errorCode = GetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			if (errorCode != 10054 && errorCode != 10053 && errorCode != 10038)
			{
				LOG(logLevel::Error, LO_TXT, "WSASend Error " + to_string(errorCode) + "\tby socket " + to_string(socket) + ", id " + to_string(ID));
				InterlockedExchange16(&sendFlag, 0);
				//int* temp = nullptr;
				//*temp = 10;
			}

			
			return false;
		}
	}

	return true;
}

/// <summary>
/// send �Ϸ� �������� ȣ��,sended ������ �ø���������� ��ȯ��. (sendFlag ��ȭ ����)
/// </summary>
/// <param name="transfer"> ���� �Ϸ����� ũ��, sendedó�� �� 0�� �Ǿ����</param>
/// <param name="sendPacketCount"> ���� �Ϸ� ��Ŷ ����, sended �������� Ȯ����</param>
/// <returns>
/// true : ������
/// false : ������ ���� (transfer zero, sended buffer corrupted, sendbuffer corrupted, WSAsend error ...)
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
/// sendBuffer�� ��Ŷ�� ������ ȣ��
/// </summary>
/// <returns>
/// true : ����
/// false : ������ ����
/// </returns>
bool session::collectSendPacket(packet& p)
{
	p.buffer->incReferenceCounter();
	////1sendBuffer.push(p.buffer); // ���� sendBuffer�� ũ�⸦ ������ ���� ����ٸ� (������ ©�󳻱� ��) �̰����� ����Ż��
	return true;

	//if (sendBuffer.freeSize() >= sizeof(serializer*))
	//{
	//	p.buffer->incReferenceCounter();
	//	auto ret = sendBuffer.push((char*)&(p.buffer), sizeof(serializer*));
	//
	//	if (ret != sizeof(serializer*))
	//	{
	//		printf("brake here");
	//	}
	//	return true;
	//}
	//else {
	//	return false;
	//}
}


/// <summary>
/// 
/// </summary>
/// <returns>
/// true : ������ ����
/// false : � �����ε� ���� ���� �Ұ���
/// </returns>
bool session::recvIO()
{
	if (hasDisconnectRequest())
		return true;

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
			if (errorCode != 10054 && errorCode != 10053 && errorCode != 10038)
			{
				LOG(logLevel::Error, LO_TXT, "WSARecv Error " + to_string(errorCode) + "\tby socket " + to_string(socket) + ", id " + to_string(ID));
				//int* temp = nullptr;
				//*temp = 10;
			}
			
			return false;
		}
		//���Ը� :: IO Cancel ���θ� Ȯ���غ���????
	}

	return true;
}

/// <summary>
/// recv �Ϸ� �������� ȣ��, recvBuffer�� �а�, ���� ����ó������
/// </summary>
/// <param name="transfer"> �Ϸ����� ���� ũ��</param>
/// <returns>
/// true : ������
/// false : ������ ���� (recvBuffer full, recvBuffer flow, transfer zero ...)
/// </returns>
bool session::recved(DWORD& transfer)
{
	if (recvBuffer.freeSize() <= transfer || transfer == 0)
		return false;

	recvBuffer.MoveRear(transfer);

	return true;
}

/// <summary>
/// recv �Ϸ� �������� ȣ��, recv ���ۿ��� ������ �ϼ��� �����Ͱ� ������ �Ű������� ����
/// </summary>
/// <returns>
/// true : ������ ����
/// false : ������ ���� 
/// </returns>
bool session::recvedPacket(packet& p)
{
	if (recvBuffer.size() >= sizeof(packet::packetHeader))
	{
		packet::packetHeader* h = (packet::packetHeader*)recvBuffer.head();
		//recvBuffer.front((char*)&h, sizeof(packet::packetHeader));

		if (recvBuffer.size() >= sizeof(packet::packetHeader) + h->size)
		{
			recvBuffer.pop((char*)p.getPacketHeader(), sizeof(packet::packetHeader) + h->size);
			p.buffer->moveRear(h->size);
			p.buffer->moveFront(-(int)sizeof(packet::packetHeader));

			return true;
		}
		else
			return false;
	}

	return false;
}
/// <summary>
/// �ش� ������ ������ ������ ���������� ���� ���� ��û�� ���, �ش� �÷��� ����
/// </summary>
/// <returns>
/// ioCount�� �ش� �÷��׸� ������ ���� ��
/// </returns>
UINT32 session::disconnectRegist()
{
	return InterlockedOr((LONG*) &IOcount, 0x80000000);
	// releaseFlag  =  �̳༮ ������������
	// disconnectFlag = IO ĵ�� ��û�� �÷���
	// ioCount = ���̿� ī��Ʈ
}

/// <summary>
/// �ش� ������ ������ ������ ���������� ���� ���� ��û�� ���, �ش� ��û���� �� �÷��� ����
/// </summary>
/// <returns>
/// ioCount�� �ش� �÷��׸� ������ ���� ��
/// </returns>
UINT32 session::disconnectUnregist()
{
	return InterlockedAnd((LONG*)&IOcount, 0x7fffffff);
}

/// <summary>
/// �ش� ������ ������ ������ ���������� ���� ���� ��û�� �������� üũ
/// </summary>
/// <returns>
/// true : ���� ��û������
/// false : �ƴ�
/// </returns>
bool session::hasDisconnectRequest()
{
	return (IOcount & 0x80000000) != 0;
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

	////1while (sendBuffer.pop(packetBuffer) != -1)
	////1{
	////1	if (packetBuffer->decReferenceCounter() == 0)
	////1		serializerFree(packetBuffer);
	////1}
	//while (!sendBuffer.empty())
	//{
	//	sendBuffer.pop((char*)&packetBuffer, sizeof(serializer*));
	//	if (packetBuffer->decReferenceCounter() == 0)
	//		serializerFree(packetBuffer);
	//}

	while (!sendedBuffer.empty())
	{
		sendedBuffer.pop((char*)&packetBuffer, sizeof(serializer*));
		if (packetBuffer->decReferenceCounter() == 0)
			serializerFree(packetBuffer);
	}

	recvBuffer.clear();
}
