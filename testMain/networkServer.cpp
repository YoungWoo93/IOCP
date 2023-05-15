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

#include "networkServer.h"

void NetworkServer::OnNewConnect(UINT64 sessionID)
{
	packet p;
	(*p.buffer) << (unsigned long long int)(0x7fffffffffffffff);
	setHeader(p);

	sendPacket(sessionID, p);
}

void NetworkServer::OnDisconnect(UINT64 sessionID)
{
	return;
}

bool NetworkServer::OnConnectionRequest(ULONG ip, USHORT port)
{
	return true;
}

void NetworkServer::OnRecv(UINT64 sessionID, packet& _packet)
{
	short h;
	UINT64 v;

	_packet >> h >> v;


	{
		packet decryptionPacket;
		int size = 0;
		decryption(_packet.buffer->getHeadPtr(), _packet.buffer->size(), decryptionPacket.buffer->getHeadPtr(), &size);
		decryptionPacket.buffer->moveRear(size);
	}


	packet p;
	(*p.buffer) << v;
	setHeader(p);

	auto ret = sendPacket(sessionID, p);
}


void NetworkServer::OnSend(UINT64 sessionID, int sendsize)
{
	return;
}


void NetworkServer::OnError(int code, const char* msg)
{
	if (errorCodeComment.find(code) == errorCodeComment.end())
		code = ERRORCODE_NOTDEFINE;

	LOG(logLevel::Error, LO_TXT, errorCodeComment[code] + msg);

	return;
}


bool NetworkServer::sendPacket(UINT64 sessionID, packet& _packet)
{
	sessionPtr s = findSession(sessionID);

	if (!s.valid())
		return false;

	packet encryptionPacket;
	int size = 0;
	encryption(_packet.buffer->getHeadPtr(), _packet.buffer->size(), encryptionPacket.buffer->getHeadPtr(), &size);
	encryptionPacket.buffer->moveRear(size);

	if (s.collectSendPacket(_packet))
		s.sendIO();

	return true;
}
