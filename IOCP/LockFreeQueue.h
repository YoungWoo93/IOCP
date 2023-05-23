#pragma once

#include <Windows.h>

template <typename T>
class LockFreeQueue
{
public:
	LockFreeQueue(size_t initailSize = 1000) : np(initailSize)
	{
		_size = 0;
		pushs = 0;
		_head = (UINT64)np.Alloc();
		((node*)_head)->key = NULL;
		_tail = _head;
	}
	~LockFreeQueue()
	{
		T temp;
		while (_head != 0) {
			node* popNode = MAKE_NODEPTR(_head);
			_head = popNode->key;

			np.Free(popNode);
		}
	}

	bool push(T t)
	{
		UINT64 pushCount = InterlockedIncrement(&pushs);
		node* n = np.Alloc();

		UINT64 newKey = MAKE_KEY(n, pushCount);
		if (newKey == nullptr)
			return false;

		n->value = t;
		n->key = NULL;

		while (true)
		{
			UINT64 tail = _tail;
			UINT64 head = _head;
			UINT64 key = MAKE_NODEPTR(tail)->key;

			if (key == NULL)
			{
				if (InterlockedCompareExchange(&(MAKE_NODEPTR(tail)->key), newKey, key) == key)
				{
					InterlockedCompareExchange(&_tail, newKey, tail);

					break;
				}
			}
			else {
				InterlockedCompareExchange(&_tail, key, tail);
			}
		}

		InterlockedIncrement(&_size);

		return true;
	}

	bool pop(T& t)
	{
		if (_size <= 0)
			return false;

		InterlockedDecrement(&_size);

		while (true)
		{
			UINT64 head = _head;
			UINT64 tail = _tail;
			UINT64 key = MAKE_NODEPTR(head)->key;

			if (key == NULL) {
				YieldProcessor();
				continue;
			}

			if (head == tail) {
				InterlockedCompareExchange(&_tail, MAKE_NODEPTR(tail)->key, tail);
				continue;
			}

			if (_head == head)
			{
				_head = key;
				np.Free(MAKE_NODEPTR(head));
				t = MAKE_NODEPTR(key)->value;

				break;
			}

		}

		return 0;
	}

	long size() {
		return _size;
	}

private:
	struct node
	{
		node() : value(0), key(0), poolNext(0) {
		}
		node(T v) :value(v), key(0), poolNext(0) {
		}
		node(T v, UINT64 k) :value(v), key(k), poolNext(0) {
		}
		~node() {
		}

		T value;
		UINT64 key;
		UINT64 poolNext;
	};

	class nodePool
	{
		friend LockFreeQueue;
	public:
		nodePool()
			:maxSize(0), useSize(0), topKey(0), heap(HeapCreate(0, 0, 0)) {

		}

		nodePool(int _size)
			: maxSize(_size), useSize(0), topKey(0), heap(HeapCreate(0, 0, 0))
		{
			for (int i = 0; i < _size; i++)
			{
				node* newNode = (node*)HeapAlloc(heap, 0, sizeof(node));
				newNode->poolNext = topKey;

				topKey = MAKE_KEY(newNode, pushCount++);
			}
		}

		~nodePool() {
			while (topKey != 0) {
				node* temp = MAKE_NODEPTR(topKey);
				topKey = temp->poolNext;

				temp->~node();
				HeapFree(heap, 0, temp);
			}

			HeapDestroy(heap);
		}


		void clear() {
			maxSize = 0;
			useSize = 0;
			pushCount = 0;

			while (topKey != nullptr) {
				node* temp = MAKE_NODEPTR(topKey);
				topKey = temp->poolNext;

				temp->~node();
				HeapFree(heap, 0, temp);

				if (topKey == nullptr)
					break;
			}
		}

		node* Alloc()
		{
			while (true)
			{
				UINT64 compareKey = topKey;
				node* popNode = MAKE_NODEPTR(compareKey);

				if (popNode == nullptr)
					return nullptr;


				if (InterlockedCompareExchange(&topKey, popNode->poolNext, compareKey) == compareKey)
				{
					InterlockedIncrement(&useSize);
					return popNode;
				}
			}
		}

		bool Free(node* pData)
		{
			node* freeNode = pData;
			UINT64 compareKey;

			while (true)
			{
				compareKey = topKey;
				freeNode->poolNext = compareKey;

				if (InterlockedCompareExchange(&topKey, MAKE_KEY(freeNode, InterlockedIncrement(&pushCount)), compareKey) == compareKey)
				{
					InterlockedDecrement(&useSize);
					return true;
				}
			}
		}

		int	GetCapacityCount(void)
		{
			return maxSize;
		}

		int	GetUseCount(void)
		{
			return useSize;
		}

	private:
		size_t maxSize;	//interlocked target
		size_t useSize;	//interlocked target
		UINT64 topKey;	//interlocked target
		UINT64 pushCount;
		HANDLE heap;

	};

	inline static UINT64 MAKE_KEY(node* nodePtr, UINT64 pushNo)
	{
		return (pushNo << 48) | (UINT64)nodePtr;
	}
	inline static node* MAKE_NODEPTR(UINT64 key)
	{
		return (node*)(0x0000FFFFFFFFFFFF & key);
	}

	UINT64 pushs;

	UINT64 _head;
	UINT64 _tail;

	nodePool np;
	long _size;

	bool resize;

};

