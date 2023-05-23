#pragma once

/*/
#define		SIZE(headPtr, tailPtr)					((tailPtr - headPtr + bufferSize) & (bufferSize - 1))
#define		MAXSIZE(headPtr, tailPtr)				(bufferSize - 1)
#define		FREESIZE(headPtr, tailPtr)				(bufferSize - 1 - ((tailPtr - headPtr + bufferSize) & (bufferSize - 1)))
#define		EMPTY(headPtr, tailPtr)					(headPtr == tailPtr)
#define		FULL(headPtr, tailPtr)					(headPtr == buffer + ((tailPtr - buffer + 1) & (bufferSize - 1)))
#define		HEAD(headPtr, tailPtr)					(buffer + ((headPtr - buffer + 1) & (bufferSize - 1)))
#define		TAIL(headPtr, tailPtr)					(buffer + ((tailPtr - buffer + 1) & (bufferSize - 1)))
#define		DIRECTENQUEUESIZE(headPtr, tailPtr)		(TAIL(headPtr, tailPtr) <= headPtr ? headPtr - TAIL(headPtr, tailPtr) : (buffer + bufferSize) - TAIL(headpPtr, tailPtr))
#define		DIRECTDEQUEUESIZE(headPtr, tailPtr)		(HEAD(headPtr, tailPtr) <= TAIL(headPtr, tailPtr) ? TAIL(headPtr, tailPtr) - HEAD(headPtr, tailPtr) : (buffer + bufferSize) - HEAD(headPtr, tailPtr))
/*/




class ringBuffer {
public:
	ringBuffer(const size_t _size = 8192);
	ringBuffer(const ringBuffer& _b);
	ringBuffer& operator =(const ringBuffer& v);

	~ringBuffer();

	inline size_t size() const {
		return (tailPtr - headPtr + bufferSize) & _maxSize;
	}
	inline size_t maxSize() const {
		return _maxSize;
	}
	inline size_t freeSize() const {
		return _maxSize - (tailPtr - headPtr + bufferSize) & _maxSize;
	}

	inline void clear() {
		headPtr = tailPtr;
	}

	inline bool empty() const {
		return headPtr == tailPtr;
	}
	inline bool full() const {
		return headPtr == buffer + ((tailPtr - buffer + 1) & _maxSize);
	}

	inline char* head()  const {
		return buffer + ((headPtr - buffer + 1) & _maxSize);
	}
	inline char* tail()  const {
		return buffer + ((tailPtr - buffer + 1) & _maxSize);
	}

	inline size_t DirectEnqueueSize() const {
		char* h = headPtr;
		char* t = tailPtr;

		if ((buffer + ((t - buffer + 1) & _maxSize)) <= h)
			return h - (buffer + ((t - buffer + 1) & _maxSize));

		return (buffer + bufferSize) - (buffer + ((t - buffer + 1) & _maxSize));
	}
	inline size_t DirectDequeueSize() const {
		char* h = headPtr;
		char* t = tailPtr;

		if ((buffer + ((h - buffer + 1) & _maxSize)) <= (buffer + ((t - buffer + 1) & _maxSize)))
			return (buffer + ((t - buffer + 1) & _maxSize)) - (buffer + ((h - buffer + 1) & _maxSize));

		return (buffer + bufferSize) - (buffer + ((h - buffer + 1) & _maxSize));
	}



	int MoveRear(const int _size);
	int MoveFront(const int _size);

	size_t	push(const char* _data, size_t _data_size);
	size_t	pop(const char* _buffer, size_t _buffer_size);
	size_t	front(const char* _buffer, size_t _buffer_size) const;

	/// <summary>
	/// µð¹ö±ë¿ë
	void printbuffer() const;
	/// </summary>
	/// 
	/// 
protected:
	inline size_t size(const char* headPtr, const char* tailPtr) const
	{
		return (tailPtr - headPtr + bufferSize) & _maxSize;
	}
	inline size_t freeSize(const char* headPtr, const char* tailPtr) const
	{
		return _maxSize - ((tailPtr - headPtr + bufferSize) & _maxSize);
	}
	inline bool empty(const char* headPtr, const char* tailPtr) const
	{
		return headPtr == tailPtr;
	}
	inline bool full(const char* headPtr, const char* tailPtr) const
	{
		return headPtr == buffer + ((tailPtr - buffer + 1) & _maxSize);
	}
	inline char* head(const char* headPtr, const char* tailPtr) const
	{
		return buffer + ((headPtr - buffer + 1) & _maxSize);
	}
	inline char* tail(const char* headPtr, const char* tailPtr) const
	{
		return buffer + ((tailPtr - buffer + 1) & _maxSize);
	}

private:
	unsigned int bufferSize;
	unsigned int _maxSize;

	char* buffer;
	char* headPtr;
	char* tailPtr;
};













