#pragma once

#ifdef _DEBUG
#pragma comment(lib, "RingBufferD")
#pragma comment(lib, "MemoryPoolD")
#pragma comment(lib, "SerializerD")
#pragma comment(lib, "MessageLoggerD")

#else
#pragma comment(lib, "RingBuffer")
#pragma comment(lib, "MemoryPool")
#pragma comment(lib, "Serializer")
#pragma comment(lib, "MessageLogger")

#endif


#include "RingBuffer/RingBuffer/RingBuffer.h"
#include "MemoryPool/MemoryPool/MemoryPool.h"
#include "Serializer/Serializer/Serializer.h"
#include "Serializer/Serializer/PacketSerializer.h"
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
		if (buffer->decReferenceCounter() == 0) {
			serializerFree(buffer);
		}
	}
	packet(packet& p) {
		buffer = p.buffer;
		buffer->incReferenceCounter();
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
		if (this->buffer->decReferenceCounter() == 0)
		{
			AcquireSRWLockExclusive(&serializerPoolLock);
			serializerPool.Free(this->buffer);
			ReleaseSRWLockExclusive(&serializerPoolLock);
		}

		p.buffer->incReferenceCounter();
		this->buffer = p.buffer;

		return *this;
	}

	packet& operator = (serializer* s) {
		if (this->buffer->decReferenceCounter() == 0)
		{
			AcquireSRWLockExclusive(&serializerPoolLock);
			serializerPool.Free(this->buffer);
			ReleaseSRWLockExclusive(&serializerPoolLock);
		}

		s->incReferenceCounter();
		this->buffer = s;

		return *this;
	}

	
};


