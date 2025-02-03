#pragma once

#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>		// SRWLOCK
#include <WinSock2.h>		// LPFN_ACCEPTEX, ntoh
#include <Mstcpip.h>		// IN6ADDR_SETV4MAPPED
#include <Mswsock.h>		// LPFN_GETACCEPTEXSOCKADDRS
#include <ws2tcpip.h>		// ADDRINFO
#include <debugapi.h>		// #error:  "No Target Architecture"
#include <process.h>		// _beginthreadex
#include <iostream>
#include <string>
// #include <set>
// #include <map>
// #include <unordered_map>
// #include <unordered_set>
// #include <queue>
#include <type_traits>		// std::is_base_of_v
// #include <format>
// #include <coroutine>

// REF: https://learn.microsoft.com/ko-kr/windows/win32/winprog/windows-data-types
#if defined(_WIN64)
typedef unsigned __int64 ULONG_PTR;
#else
typedef unsigned long ULONG_PTR;
#endif

typedef unsigned char BYTE;
#define BYTE_MAX	0xff

typedef ULONG_PTR SIZE_T;
typedef short SHORT;
typedef unsigned short USHORT;
#define USHORT_MAX     0xffff

#define MS_TO_SEC		0.001f
#define SEC_TO_MS		1000.0f

using NetConnID = USHORT;
extern const NetConnID INVALID_CONNECTION;
using SenderID = NetConnID;

// REF: https://stackoverflow.com/questions/32432450/what-is-standard-defer-finalizer-implementation-in-c
struct defer_dummy {};
template <class F> struct deferrer { F f; ~deferrer() { f(); } };
template <class F> deferrer<F> operator*(defer_dummy, F f) { return { f }; }
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()

#define LOCK_EXCLUSIVE(lock)	ScopedLockExclusive _lambdaLock1(&lock);
#define LOCK_SHARED(lock)		ScopedLockShared _lambdaLock2(&lock);

class ScopedLockExclusive
{
public:
	inline ScopedLockExclusive()
		: ScopedLockExclusive(nullptr)
	{}

	inline ScopedLockExclusive(SRWLOCK* pSRWLock)
		: m_pLock(pSRWLock)
	{
		if (m_pLock != nullptr)
			AcquireSRWLockExclusive(m_pLock);
	}

	inline virtual ~ScopedLockExclusive()
	{
		if (m_pLock != nullptr)
			ReleaseSRWLockExclusive(m_pLock);
	}

	inline void Init(SRWLOCK* pLock)
	{
		assert(m_pLock == nullptr);
		assert(pLock != nullptr);

		m_pLock = pLock;
	}

private:
	SRWLOCK* m_pLock;
};

class ScopedLockShared
{
public:
	inline ScopedLockShared(SRWLOCK* pSRWLock)
		: m_pLock(pSRWLock)
	{
		assert(m_pLock != nullptr);

		AcquireSRWLockShared(m_pLock);
	}

	inline virtual ~ScopedLockShared()
	{
		assert(m_pLock != nullptr);

		ReleaseSRWLockShared(m_pLock);
	}

private:
	SRWLOCK* m_pLock;

};

// Ref: https://github.com/megayuchi/PerformanceTest/blob/main/PerfTest2/QueryPerfCounter.h

void QCInit();
LARGE_INTEGER QCGetCounter();
float QCMeasureElapsedTick(LARGE_INTEGER curCounter, LARGE_INTEGER prvCounter);		// 단위: ms
LARGE_INTEGER QCCounterAddTick(LARGE_INTEGER counter, float tick);					// 단위: ms
LARGE_INTEGER QCCounterSubTick(LARGE_INTEGER counter, float tick);					// 단위: ms

void ntohwstr(std::wstring_view wstr);
void htonwstr(std::wstring_view wstr);

void LogError(const char* file, const char* func, int line, const char* format, ...);
#define LOG_ERROR(formatStr, ...)		{ LogError((__FILE__), (__FUNCTION__), (__LINE__), (formatStr), ##__VA_ARGS__); }
#define LOG_ERROR_STOP(formatStr, ...)	{ LogError((__FILE__), (__FUNCTION__), (__LINE__), (formatStr), ##__VA_ARGS__); DebugBreak(); }

/*
struct StringViewCompare {
	using is_transparent = void;

	inline bool operator()(const std::string& lhs, const std::string& rhs) const {
		return lhs < rhs;
	}

	inline bool operator()(const std::string& lhs, const std::string_view rhs) const {
		return lhs < rhs;
	}

	inline bool operator()(const std::string_view lhs, const std::string& rhs) const {
		return lhs < rhs;
	}

	inline bool operator()(const std::string_view lhs, const std::string_view rhs) const {
		return lhs < rhs;
	}
};

struct WStringViewCompare {
	using is_transparent = void;

	inline bool operator()(const std::wstring& lhs, const std::wstring& rhs) const {
		return lhs < rhs;
	}

	inline bool operator()(const std::wstring& lhs, const std::wstring_view rhs) const {
		return lhs < rhs;
	}

	inline bool operator()(const std::wstring_view lhs, const std::wstring& rhs) const {
		return lhs < rhs;
	}

	inline bool operator()(const std::wstring_view lhs, const std::wstring_view rhs) const {
		return lhs < rhs;
	}
};

// deprecated: buffer/Hashmap으로 대체
template <typename ValueType>
class StringMap
{
public:
	using Map = std::map<std::string, ValueType, StringViewCompare>;
	using Pair = std::pair<const std::string_view, ValueType>;			// erase_if pred param
	using Iterator = std::map<std::string, ValueType, StringViewCompare>::iterator;

public:
	ValueType& Set(std::string_view key, const ValueType& value)
	{
		return (m_map[std::string(key)] = value);
	}

	ValueType& Set(std::string_view key, ValueType&& value)
	{
		return (m_map[std::string(key)] = std::forward<ValueType>(value));
	}

	ValueType& Get(std::string_view key)
	{
		auto iter = m_map.find(key);
		assert(iter != m_map.end());

		return iter->second;
	}

	ValueType* TryGet(std::string_view key)
	{
		auto iter = m_map.find(key);
		if (iter == m_map.end())
			return nullptr;

		return &(iter->second);
	}

	bool Contains(std::string_view key)
	{
		auto iter = m_map.find(key);

		return iter != m_map.end();
	}

	bool Contains(std::string_view* pKey)
	{
		assert(pKey != nullptr);

		auto iter = m_map.find(*pKey);

		return iter != m_map.end();
	}

	const std::string& GetKeyStringRef(std::string_view key)
	{
		auto iter = m_map.find(key);
		assert(iter != m_map.end());

		return iter->first;
	}

	void Delete(std::string_view key)
	{
		auto iter = m_map.find(key);
		assert(iter != m_map.end());

		m_map.erase(iter);
	}

	const Map& GetMap()
	{
		return m_map;
	}

	SIZE_T GetCount()
	{
		return m_map.size();
	}

private:
	Map m_map;

};

// deprecated: buffer/Hashmap으로 대체
template <typename ValueType>
class WideStringMap
{
public:
	using Map = std::map<std::wstring, ValueType, WStringViewCompare>;
	using Pair = std::pair<const std::wstring_view, ValueType>;			// erase_if pred param
	using Iterator = std::map<std::wstring, ValueType, WStringViewCompare>::iterator;

public:
	ValueType* TrySet(std::wstring_view key)
	{
		if (m_map.contains(key))
			return nullptr;

		ValueType& ret = (m_map[std::wstring(key)] = ValueType());

		return &ret;
	}

	ValueType& Set(std::wstring_view key, const ValueType& value)
	{
		return (m_map[std::wstring(key)] = value);
	}

	ValueType& Set(std::wstring_view key, ValueType&& value)
	{
		return (m_map[std::wstring(key)] = std::forward<ValueType>(value));
	}

	ValueType& Get(std::wstring_view key)
	{
		auto iter = m_map.find(key);
		return iter->second;
	}

	bool Contains(std::wstring_view key)
	{
		auto iter = m_map.find(key);

		return iter != m_map.end();
	}

	void Delete(std::wstring_view key)
	{
		auto iter = m_map.find(key);
		assert(iter != m_map.end());

		m_map.erase(iter);
	}

	Map& GetMap()
	{
		return m_map;
	}

	SIZE_T GetCount()
	{
		return m_map.size();
	}

private:
	Map m_map;

};
*/

class AtomicBool
{
public:
	AtomicBool(bool bInitVal);

	// 이전 값을 리턴한다. 
	bool Set(bool bNew);

	// 이전 값을 리턴한다. 
	bool Cas(bool bExpected, bool bSet);

	bool Get();

private:
	volatile LONG m_bool;

};
