#include "ringBuffer.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <limits.h>
#include <algorithm> // for min(a, b)


ringBuffer::ringBuffer(const size_t _size)
	:bufferSize(static_cast<int>(pow(2, ceil(log2(static_cast<double>(_size)))))),
	_maxSize(bufferSize - 1),
	buffer(new char[bufferSize]),
	headPtr(buffer),
	tailPtr(buffer) {
}

ringBuffer::ringBuffer(const ringBuffer& _b)
	:bufferSize(_b.bufferSize),
	_maxSize(bufferSize - 1),
	buffer(new char[_b.bufferSize]),
	headPtr(buffer + (_b.headPtr - _b.buffer)),
	tailPtr(buffer + (_b.tailPtr - _b.buffer)) {

	if (headPtr <= tailPtr)
		memmove(head(), _b.head(), _b.size());
	else
	{
		memmove(head(), _b.head(), _b.DirectDequeueSize());
		memmove(buffer, _b.buffer, _b.size() - _b.DirectDequeueSize());
	}
}

ringBuffer& ringBuffer::operator =(const ringBuffer& _b) {
	delete[] buffer;
	bufferSize = _b.bufferSize;
	_maxSize = _b._maxSize;
	buffer = new char[_b.bufferSize];
	headPtr = buffer + (_b.headPtr - _b.buffer);
	tailPtr = buffer + (_b.tailPtr - _b.buffer);

	if (headPtr <= tailPtr)
		memmove(head(), _b.head(), _b.size());
	else
	{
		memmove(head(), _b.head(), _b.DirectDequeueSize());
		memmove(buffer, _b.buffer, _b.size() - _b.DirectDequeueSize());
	}

	return *this;
}

ringBuffer::~ringBuffer() {
	delete[] buffer;
}

int ringBuffer::MoveRear(int _size) {
	char* h = headPtr;
	char* t = tailPtr;

	int tempF = static_cast<int>(std::min(freeSize(h, t), static_cast<size_t>(INT_MAX)));
	if (_size > tempF)
		_size = tempF;

	intptr_t offset = reinterpret_cast<intptr_t>(t) - reinterpret_cast<intptr_t>(buffer);
	offset = (offset + _size) & _maxSize;

	tailPtr = buffer + offset;

	return _size;
}
int ringBuffer::MoveFront(int _size) {
	char* h = headPtr;
	char* t = tailPtr;


	int tempS = static_cast<int>(std::min(size(h, t), static_cast<size_t>(INT_MAX)));
	if (_size > tempS)
		_size = tempS;

	intptr_t offset = reinterpret_cast<intptr_t>(h) - reinterpret_cast<intptr_t>(buffer);
	offset = (offset + _size) & _maxSize;

	headPtr = buffer + offset;

	return _size;
}

size_t ringBuffer::push(const char* _data, size_t _data_size) {
	char* h = headPtr;
	char* t = tailPtr;

	size_t tempF = freeSize(h, t);
	if (_data_size > tempF)
		_data_size = tempF;

	char* tempT = tail(h, t);
	size_t d;
	if (tempT <= h)
		d = h - tempT;
	else
		d = (buffer + bufferSize) - tempT;


	if (d >= _data_size)
		memmove(tempT, const_cast<char*>(_data), _data_size);
	else
	{
		memmove(tempT, const_cast<char*>(_data), d);
		memmove(buffer, const_cast<char*>(_data + d), _data_size - d);
	}


	{
		intptr_t offset = reinterpret_cast<intptr_t>(t) - reinterpret_cast<intptr_t>(buffer);
		offset = (offset + _data_size) & _maxSize;

		tailPtr = buffer + offset;
	}

	return _data_size;
}
size_t ringBuffer::pop(const char* _buffer, size_t _buffer_size) {
	char* h = headPtr;
	char* t = tailPtr;


	size_t tempS = size(h, t);
	if (_buffer_size > tempS)
		_buffer_size = tempS;

	char* tempT = tail(h, t);
	char* tempH = head(h, t);

	size_t d;
	if (tempH <= tempT)
		d = tempT - tempH;
	else
		d = (buffer + bufferSize) - tempH;


	if (d >= _buffer_size)
		memmove(const_cast<char*>(_buffer), tempH, _buffer_size);
	else
	{
		memmove(const_cast<char*>(_buffer), tempH, d);
		memmove(const_cast<char*>(_buffer + d), buffer, _buffer_size - d);
	}


	{
		intptr_t offset = reinterpret_cast<intptr_t>(h) - reinterpret_cast<intptr_t>(buffer);
		offset = (offset + _buffer_size) & _maxSize;

		headPtr = buffer + offset;
	}

	return _buffer_size;
}

size_t ringBuffer::front(const char* _buffer, size_t _buffer_size) const {
	char* h = headPtr;
	char* t = tailPtr;

	size_t tempS = size(h, t);
	if (_buffer_size > tempS)
		_buffer_size = tempS;

	char* tempT = tail(h, t);
	char* tempH = head(h, t);

	size_t d;
	if (tempH <= tempT)
		d = tempT - tempH;
	else
		d = (buffer + bufferSize) - tempH;


	if (d >= _buffer_size)
		memmove(const_cast<char*>(_buffer), tempH, _buffer_size);
	else
	{
		memmove(const_cast<char*>(_buffer), tempH, d);
		memmove(const_cast<char*>(_buffer + d), buffer, _buffer_size - d);
	}

	return _buffer_size;
}


void ringBuffer::printbuffer() const
{
	for (unsigned int i = 0; i < bufferSize; i++)
		printf("==");
	printf("\n");

	for (unsigned int i = 0; i < bufferSize; i++)
	{
		if (buffer + i == head())
			printf(" ->");
		else
			printf("  ");
	}
	printf("\n");

	for (unsigned int i = 0; i < bufferSize; i++)
		printf(" %c", *(buffer + i));
	printf("\n");

	for (unsigned int i = 0; i < bufferSize; i++)
	{
		if (buffer + i == tail())
			printf("-|");
		else
			printf("  ");
	}
	printf("\n");

	for (unsigned int i = 0; i < bufferSize; i++)
		printf("==");
	printf("\n");
}