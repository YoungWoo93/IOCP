#pragma once

#include <string>
#include <Windows.h>
#include <stdexcept>

class packetSerializer;




class serializer
{
public:
#pragma pack(push, 1)
	struct packetHeader {
		char code;
		short size;
		char randKey;
		char checkSum;
	};
#pragma pack(pop)

	serializer() : bufferPtr((char*)_aligned_malloc(1024, 64)), bufferSize(1024), headPtr(bufferPtr), tailPtr(bufferPtr), useHeap(false), heap(0), referenceCounter(0) {	// c++ allocator 를 이용해 64바이트 (캐시라인) 경계에 서도록 짜보자

	}																					//    aligned alloc 구현 이후
	serializer(const size_t _size)
		:bufferPtr((char*)_aligned_malloc(_size, 64)), bufferSize(_size), headPtr(bufferPtr), tailPtr(bufferPtr), useHeap(false), heap(0), referenceCounter(0) {
	}
	serializer(HANDLE heap)
		:bufferPtr((char*)HeapAlloc(heap, NULL, 1024)), bufferSize(1024), headPtr(bufferPtr), tailPtr(bufferPtr), useHeap(true), heap(heap), referenceCounter(0) {
	}
	serializer(HANDLE heap, const size_t _size)
		:bufferPtr((char*)HeapAlloc(heap, NULL, _size)), bufferSize(_size), headPtr(bufferPtr), tailPtr(bufferPtr), useHeap(true), heap(heap), referenceCounter(0) {
	}
	serializer(const serializer& s)
		:bufferPtr((char*)_aligned_malloc(s.bufferSize, 64))
		, bufferSize(s.bufferSize)
		, headPtr(bufferPtr + (s.headPtr - s.bufferPtr))
		, tailPtr(bufferPtr + (s.tailPtr - s.bufferPtr))
		, useHeap(s.useHeap)
		, heap(s.heap)
		, referenceCounter(0) {
		memmove(headPtr, s.headPtr, s.tailPtr - s.headPtr);
	}

	~serializer() {
		if (useHeap)
			HeapFree(heap, 0, bufferPtr);
		else
			_aligned_free(bufferPtr);
	}

	size_t push(const char* _data, size_t _size);
	size_t pop(char* _data, size_t _size);
	size_t peek(char* _buffer, size_t _size);

	void clean();
	void clear();
	char* resize(const size_t _size);

	char* getBufferPtr();
	size_t getBufferSize();
	char* getHeadPtr();
	char* getTailPtr();

	int moveFront(const int _size);
	int moveRear(const int _size);

	inline int incReferenceCounter(int size = 1)
	{
		return InterlockedAdd(&referenceCounter, size);
	}

	inline int decReferenceCounter(int size = 1)
	{
		return InterlockedAdd(&referenceCounter, -size);
	}

	inline int setReferenceCounter(int count)
	{
		return InterlockedExchange(&referenceCounter, count);
	}

	const inline size_t size() {
		return tailPtr - headPtr;
	}

	const inline size_t useableSize() {
		return bufferSize - (tailPtr - bufferPtr);
	}

	const inline size_t maxSize() {
		return bufferSize;
	}

	const inline size_t usedSize() {
		return headPtr - bufferPtr;
	}

	

	volatile LONG referenceCounter;
protected:
	char* bufferPtr;
	size_t bufferSize;

	char* headPtr;
	char* tailPtr;

	bool useHeap;
	HANDLE heap;

public:

	/*/ basic data type /*/

	serializer& operator << (const bool v);  // 1비트 처리로 인한 byte 경계 훼손이 더 문제인듯 입력이되면 에러를 뽑는게 더 나으려나?

	serializer& operator << (const char v);
	serializer& operator << (const unsigned char v);
	serializer& operator << (const wchar_t v);

	serializer& operator << (const short v);
	serializer& operator << (const unsigned short v);
	serializer& operator << (const int v);
	serializer& operator << (const unsigned int v);
	serializer& operator << (const long v);
	serializer& operator << (const unsigned long v);
	serializer& operator << (const long long v);
	serializer& operator << (const unsigned long long v);

	serializer& operator << (const float v);
	serializer& operator << (const double v);

	serializer& operator << (const std::string& v);
	serializer& operator >> (std::string& v);



	serializer& operator >> (bool& v);

	serializer& operator >> (char& v);
	serializer& operator >> (unsigned char& v);
	serializer& operator >> (wchar_t& v);

	serializer& operator >> (short& v);
	serializer& operator >> (unsigned short& v);
	serializer& operator >> (int& v);
	serializer& operator >> (unsigned int& v);
	serializer& operator >> (long& v);
	serializer& operator >> (unsigned long& v);
	serializer& operator >> (long long& v);
	serializer& operator >> (unsigned long long& v);

	serializer& operator >> (float& v);
	serializer& operator >> (double& v);
	//



/*/ custom data type //

/*/
	friend class packetSerializer;

	virtual serializer& operator = (const serializer& v);
};









