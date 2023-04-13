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
#include "localServer.h"

void LocalServer::OnNewConnect(UINT64 sessionID)
{
	return;
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
	return;
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