#pragma once
#include <map>

#include "IOCP/IOCP/network.h"
#include "IOCP/IOCP/session.h"

class NetworkClient : public Network {
public:
	NetworkClient();
	~NetworkClient();

	bool start(const std::map<std::string, std::pair<std::string, USHORT>>& serverList);
	void stop();
	static DWORD WINAPI workerThread(LPVOID arg);
private:
	virtual void OnNewConnect(UINT64 sessionID);
	virtual void OnDisconnect(UINT64 sessionID);
	virtual bool OnConnectionRequest(ULONG ip, USHORT port);

	virtual void OnRecv(UINT64 sessionID, packet& _packet);	// < 패킷 수신 완료 후
	virtual void OnSend(UINT64 sessionID, int sendsize);       // < 패킷 송신 완료 후

	virtual void OnError(int errorcode, const char* msg = "");


public:
	std::map<std::string, UINT64> infoIDMap;
	std::map<UINT64, std::string> IDInfoMap;
};