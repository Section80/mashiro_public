#pragma once

#include "buffer/LinkedList.h"

struct RawBuffer
{
	void* pBuffer;
	int		byteSize;

	UINT				GetHashCode();
	std::string_view	GetStringView();
	std::wstring_view	GetWStringView();

};

struct HashIndex
{
	LinkedList	entryList;
	Link		occupiedLink;

};

struct HashEntry
{
	int		keyByteSize;
	void*	pValue;

	Link	indexLink;		// iterate entry in same hashIndex

	RawBuffer GetKey();

};

struct HashEntryIndex
{
	HashIndex* pIndex;
	HashEntry* pEntry;

	RawBuffer Getkey();
	std::string_view GetKeyString();
	std::wstring_view GetKeyWString();
	void* GetValue();

};

class HashMap
{
public:
	HashMap();
	virtual ~HashMap();

	void Init(HashIndex* pTable, int tableSize);
	void Init(int tableSize);
	void CleanUp();

	HashEntry* Insert(RawBuffer key, void* pValue);
	HashEntry* Insert(void* pKey, ULONGLONG keySize, void* pValue);
	HashEntry* Insert(std::string_view key, void* pValue);
	HashEntry* Insert(std::wstring_view key, void* pValue);
	
	void* GetValue(RawBuffer key);
	void* GetValue(void* pKey, ULONGLONG keySize);
	void* GetValue(std::string_view key);
	void* GetValue(std::wstring_view key);

	HashEntry* GetEntry(RawBuffer key);
	HashEntry* GetEntry(void* pKey, ULONGLONG keySize);
	HashEntry* GetEntry(std::string_view key);
	HashEntry* GetEntry(std::wstring_view key);

	bool Delete(RawBuffer key);
	bool Delete(void* pKey, ULONGLONG keySize);
	bool Delete(std::string_view key);
	bool Delete(std::wstring_view key);

	HashEntryIndex GetIndex(int index);
	HashEntryIndex GetNextIndex(HashEntryIndex& entryIndex);

	int GetEntryCount();

private:
	int				m_tableSize;
	HashIndex*		m_pTable;

	bool			m_bShouldReleaseTable;

	LinkedList		m_occupiedList;
	int				m_entryCount;

};
