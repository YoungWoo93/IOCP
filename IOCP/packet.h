#pragma once

#ifdef _DEBUG
#pragma comment(lib, "MemoryPoolD")
#pragma comment(lib, "MessageLoggerD")

#else
#pragma comment(lib, "MemoryPool")
#pragma comment(lib, "MessageLogger")

#endif

#include "serializer.h"

#include "Serializer/Serializer/BaseSerializer.h"
#include "MemoryPool/MemoryPool/MemoryPool.h"
#include "MessageLogger/MessageLogger/MessageLogger.h"

extern ObjectPool<serializer> serializerPool;
extern SRWLOCK serializerPoolLock;
class packet;

std::pair<size_t, size_t> packetPoolMemoryCheck();
void serializerFree(serializer* s);
serializer* serializerAlloc();
void setHeader(packet& p);


class packet {
public:
#pragma pack(push, 1)
	struct packetHeader {
		char code;
		short size;
		char randKey;
		char checkSum;
	};
#pragma pack(pop)
public:
	serializer* buffer;

	packet() {
		buffer = serializerAlloc();
		buffer->clean();

		buffer->incReferenceCounter();
		buffer->moveRear(sizeof(packetHeader));
		buffer->moveFront(sizeof(packetHeader));
	}

	~packet() {
		int temp = buffer->decReferenceCounter();
		if (temp < 0)
		{
			cout << "error" << endl;
		}
		if (temp == 0) {
			serializerFree(buffer);
		}

	}
	packet(packet& p) {
		buffer = p.buffer;
		buffer->incReferenceCounter();
	}

	int incReferenceCounter(int size = 1)
	{
		return buffer->incReferenceCounter(size);
	}

	int decReferenceCounter(int size = 1)
	{
		int ret = buffer->decReferenceCounter(size);
		if (ret == 0)
		{
			AcquireSRWLockExclusive(&serializerPoolLock);
			serializerPool.Free(this->buffer);
			ReleaseSRWLockExclusive(&serializerPoolLock);
			buffer = nullptr;
		}

		return ret;
	}


	packetHeader* getPacketHeader()
	{
		return (packetHeader*)buffer->getBufferPtr();
	}

	char* getPayload()
	{
		return buffer->getBufferPtr() + sizeof(packetHeader);
	}

	packet& operator << (const char v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const unsigned char v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const wchar_t v) {
		(*buffer) << v;
		return *this;
	}

	packet& operator << (const short v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const unsigned short v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const int v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const unsigned int v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const long v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const unsigned long v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const long long v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const unsigned long long v) {
		(*buffer) << v;
		return *this;
	}

	packet& operator << (const float v) {
		(*buffer) << v;
		return *this;
	}
	packet& operator << (const double v) {
		(*buffer) << v;
		return *this;
	}



	packet& operator >> (char& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (unsigned char& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (wchar_t& v) {
		(*buffer) >> v;
		return *this;
	}

	packet& operator >> (short& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (unsigned short& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (int& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (unsigned int& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (long& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (unsigned long& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (long long& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (unsigned long long& v) {
		(*buffer) >> v;
		return *this;
	}

	packet& operator >> (float& v) {
		(*buffer) >> v;
		return *this;
	}
	packet& operator >> (double& v) {
		(*buffer) >> v;
		return *this;
	}

	packet& operator = (packet& p) {
		int temp = this->buffer->decReferenceCounter();

		if (temp == 0)
		{
			if (temp < 0)
			{
				cout << "error" << endl;
			}

			AcquireSRWLockExclusive(&serializerPoolLock);
			serializerPool.Free(this->buffer);
			ReleaseSRWLockExclusive(&serializerPoolLock);
		}

		p.buffer->incReferenceCounter();
		this->buffer = p.buffer;

		return *this;
	}

	packet& operator = (serializer* s) {
		int temp = this->buffer->decReferenceCounter();

		if (temp == 0)
		{
			if (temp < 0)
			{
				cout << "error" << endl;
			}

			AcquireSRWLockExclusive(&serializerPoolLock);
			serializerPool.Free(this->buffer);
			ReleaseSRWLockExclusive(&serializerPoolLock);
		}

		s->incReferenceCounter();
		this->buffer = s;

		return *this;
	}


};


