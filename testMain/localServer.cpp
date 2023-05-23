#ifdef _DEBUG
#pragma comment(lib, "MemoryPoolD")
#pragma comment(lib, "SerializerD")
#pragma comment(lib, "MessageLoggerD")
#pragma comment(lib, "crashDumpD")
#pragma comment(lib, "IOCPD")

#else
#pragma comment(lib, "MemoryPool")
#pragma comment(lib, "Serializer")
#pragma comment(lib, "MessageLogger")
#pragma comment(lib, "crashDump")
#pragma comment(lib, "IOCP")

#endif

#include "IOCP/IOCP/network.h"
#include "IOCP/IOCP/session.h"

#include "LocalServer.h"

void LocalServer::OnNewConnect(UINT64 sessionID)
{
	packet p;
	(*p.buffer) << (unsigned long long int)(0x7fffffffffffffff);
	setHeader(p);

	sendPacket(sessionID, p);
}

void LocalServer::OnDisconnect(UINT64 sessionID)
{
	return;
}

bool LocalServer::OnConnectionRequest(ULONG ip, USHORT port)
{
	return true;
}

void LocalServer::OnRecv(UINT64 sessionID, packet& _packet)
{
	short h;
	UINT64 v;

	_packet >> h >> v;

	packet p;
	(*p.buffer) << v;
	setHeader(p);

	auto ret = sendPacket(sessionID, p);
}


void LocalServer::OnSend(UINT64 sessionID, int sendsize)
{
	return;
}


void LocalServer::OnError(int code, const char* msg)
{
	if (errorCodeComment.find(code) == errorCodeComment.end())
		code = ERRORCODE_NOTDEFINE;

	LOG(logLevel::Error, LO_TXT, errorCodeComment[code] + msg);

	return;
}