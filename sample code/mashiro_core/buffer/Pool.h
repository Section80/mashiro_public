#pragma once

#include "buffer/LinkedList.h"

class Pool final
{
public:
	Pool();
	~Pool();

	void Init(int entrySize, int blockSize);
	void CleanUp();

	void* Create();
	void* Create(SIZE_T entrySize);
	void Free(void* pEntry);

	int GetCreatedEntryCount();

private:
	bool allocBlock();

private:
	int					m_entrySize;
	int					m_blockSize;

	void*				m_pNext;
	LinkedList			m_blockList;

	int					m_createdEntryCount;

};
