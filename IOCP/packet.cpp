#include "packet.h"



ObjectPool<serializer> serializerPool(100, 1000);
SRWLOCK serializerPoolLock;

std::pair<size_t, size_t> packetPoolMemoryCheck()
{
	std::pair<size_t, size_t> ret;
	AcquireSRWLockExclusive(&serializerPoolLock);
	ret.first = serializerPool.GetCapacityCount();
	ret.second = serializerPool.GetUseCount();
	ReleaseSRWLockExclusive(&serializerPoolLock);

	return ret;
}


void serializerFree(serializer* s)
{
	//AcquireSRWLockExclusive(&serializerPoolLock);
	serializerPool.Free(s);
	//ReleaseSRWLockExclusive(&serializerPoolLock);
}


serializer* serializerAlloc()
{
	//AcquireSRWLockExclusive(&serializerPoolLock);
	auto ret = serializerPool.Alloc();
	//ReleaseSRWLockExclusive(&serializerPoolLock);
	ret->clear(); //여기서 ref count를 0으로 초기화하지는 않는다, 즉 반환할때 0임을 확인하고 반환해야한다. (수정 해야 할 수도 있음)

	return ret;
}

void setHeader(packet& p)
{
	packet::packetHeader h;
	h.code = 0x77;
	h.size = (short)p.buffer->size();
	p.buffer->moveFront(-(long long int)(sizeof(packet::packetHeader)));

	//memcpy_s(p.buffer->getHeadPtr(), sizeof(packetHeader), &h, sizeof(packetHeader));
	memcpy_s(p.buffer->getHeadPtr(), sizeof(h.code) + sizeof(h.size), &h, sizeof(h.code) + sizeof(h.size));
}

void setHeader(serializer* p)
{
	serializer::packetHeader h;
	h.code = 0x77;
	h.size = (short)p->size();
	p->moveFront(-(long long int)(sizeof(packet::packetHeader)));

	//memcpy_s(p.buffer->getHeadPtr(), sizeof(packetHeader), &h, sizeof(packetHeader));
	memcpy_s(p->getHeadPtr(), sizeof(h.code) + sizeof(h.size), &h, sizeof(h.code) + sizeof(h.size));
}