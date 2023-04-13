#pragma once
#include "IOCP/IOCP/network.h"
#include "IOCP/IOCP/session.h"

class NetworkServer : public Network {
public:
	virtual void OnNewConnect(UINT64 sessionID);

	virtual void OnDisconnect(UINT64 sessionID);
	virtual bool OnConnectionRequest(ULONG ip, USHORT port);
	virtual void OnRecv(UINT64 sessionID, packet& _packet);

	virtual void OnSend(UINT64 sessionID, int sendsize);


	virtual void OnError(int code, const char* msg = "");
};