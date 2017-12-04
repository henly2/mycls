/*************************************************************************
【文件名】			UniLock.hpp
【功能模块和目的】		互斥锁	
从2修改一处		
**************************************************************************/
#pragma once

#include <Windows.h>
#include <assert.h>

namespace util{

/******************************************************
本进程使用
从1修改
add comment from 1
******************************************************/
class UniLock
{
private:
	CRITICAL_SECTION _stLock;

public:
	UniLock(void)
	{
		InitializeCriticalSection(&_stLock);
	}
	~UniLock(void)
	{
		DeleteCriticalSection(&_stLock);
	}
	void lock()
	{
		EnterCriticalSection(&_stLock);
	}
	void unlock()
	{
		LeaveCriticalSection(&_stLock);
	}
};
// 自动释放
// add comment from 1 at 2
// add modify by mater
class AutoUniLocker{
public:
	AutoUniLocker(UniLock& locker)
		:locker_(locker)
	{
		locker_.lock();
	}
	~AutoUniLocker()
	{
		locker_.unlock();
	}

private:
	UniLock& locker_;
};

//////////////////////////////////////////////////////////////////////////
/******************************************************
本进程或者跨进程互斥
******************************************************/
static const char* g_UUID_GlobalUniLock = "Global\\Hanlder_{275B04F0-6CCF-462C-8B7F-3FB922C72C7C}";
class GlobalUniLock
{
private:
	HANDLE _ach;
	bool _is_event;
public:
	GlobalUniLock(bool is_event = false)
	{
		_ach = NULL;
		_is_event = is_event;
	}
	~GlobalUniLock(void)
	{
	}
	void lock()
	{
		if (_is_event)
		{
			lock_event();
		}
		else
		{
			lock_mutex();
		}
	}
	void unlock()
	{
		if (_is_event)
		{
			unlock_event();
		}
		else
		{
			unlock_mutex();
		}
	}

protected:
	void lock_event()
	{
		// create with ownership
		_ach = ::CreateEventA( NULL, FALSE, FALSE, g_UUID_GlobalUniLock);
		if (_ach == NULL)
		{
			assert(FALSE);
			return;
		}
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			// exist, waiting ownership
			::WaitForSingleObject(_ach, INFINITE);
		}
	}
	void unlock_event()
	{
		if (_ach != NULL)
		{
			::SetEvent(_ach);
			::CloseHandle(_ach);
			_ach = NULL;
		}
	}
	void lock_mutex()
	{
		// create with ownership
		_ach = ::CreateMutexExA( NULL, g_UUID_GlobalUniLock, CREATE_MUTEX_INITIAL_OWNER, MUTEX_ALL_ACCESS);
		if (_ach == NULL)
		{
			assert(FALSE);
			return;
		}
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			// exist, waiting ownership
			::WaitForSingleObject(_ach, INFINITE);
		}
	}
	void unlock_mutex()
	{
		if (_ach != NULL)
		{
			::ReleaseMutex(_ach);
			::CloseHandle(_ach);
			_ach = NULL;
		}
	}
};
// 自动释放
class AutoGlobalUniLocker{
public:
	AutoGlobalUniLocker(GlobalUniLock& locker)
		:locker_(locker)
	{
		locker_.lock();
	}
	~AutoGlobalUniLocker()
	{
		locker_.unlock();
	}

private:
	GlobalUniLock& locker_;
};

}

#define AUTOUniLock(lock) util::AutoUniLocker _tmp_unilock(lock);
#define AUTOGlobalUniLock(lock) util::AutoGlobalUniLocker _tmp_globalunilock(lock);