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
	
	bool sendPacket(UINT64 sessionID, packet& _pakcet);

	inline char getRandKey()
	{
		return rand() & 0x000000FF;
	}

	inline char getStatickey()
	{
		return 0x32; // 50
	}

	void encryption(const char* input, size_t inputSize, char* output, int* outputSize)
	{
		char checkSum = 0;
		char dynamicKey = getRandKey();
		char staticKey = getStatickey();

		for (int i = 0; i < inputSize; i++)
			checkSum += input[i];

		char p = 0;
		output[0] = dynamicKey;
		output[1] = checkSum;

		p = input[0] ^ (dynamicKey + 1);
		output[2] = p ^ (staticKey + 1);

		for (int i = 1; i < inputSize; i++)
		{
			p = input[i] ^ (p + dynamicKey + 1 + i);
			output[2 + i] = p ^ (output[1 + i] + staticKey + 1 + i);
		}
		output[2 + inputSize] = '\0';
		*outputSize = inputSize + 2;
	}

	bool decryption(const char* input, size_t size, char* output, int* outputSize)
	{
		char key = input[0];
		char checkSum = input[1];
		char staticKey = getStatickey();

		char p;
		char pastp;

		p = input[2] ^ (staticKey + 1);
		output[0] = p ^ (key + 1);

		for (int i = 1; i < size - 2; i++)
		{
			pastp = p;
			p = input[2 + i] ^ (input[1 + i] + staticKey + 1 + i);
			output[i] = p ^ (pastp + key + 1 + i);
		}
		*outputSize = size - 2;

		for (int i = 0; i < *outputSize; i++)
			checkSum -= output[i];



		return checkSum == 0;
	}
};