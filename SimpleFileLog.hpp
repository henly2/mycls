/** 
 * @file FileLog.hpp
 * @brief 
 * @sa null
 * @author liuheng
 * @date 4/9/2017
 *20180115
 * 文件日志
 */
#ifndef simplefilelog_hpp_
#define simplefilelog_hpp_

#include "UniLock.hpp"

namespace util{

enum LOG_LEVEL{
	LL_Err = 1,
	LL_Warning,
	LL_Info,
	LL_Debug,
};

class FileLog 
{
public:
	static FileLog& Ins()
	{
		static FileLog ss;
		return ss;
	}

	bool Init()
	{
		if (INVALID_HANDLE_VALUE != file_)
		{
			return true;
		}

		current_Processid_ = GetCurrentProcessId();

		char path[MAX_PATH], path1[MAX_PATH];
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		//char ext[_MAX_EXT];
		GetModuleFileNameA(NULL, path, _MAX_PATH);
		_splitpath_s( path, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, NULL, 0);
		_makepath_s( path1, MAX_PATH, drive, dir, "log\\", NULL );
		CreateDirectoryA(path1, NULL);
		char name[MAX_PATH];
		SYSTEMTIME st;
		GetLocalTime(&st);
		sprintf_s(name, sizeof(name), "%s_%02d%02d_%02d%02d%02d.log", 
			fname, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		strcat_s(path1, MAX_PATH, name);

		AUTOUniLock(lock_);
		file_ = CreateFileA(path1, GENERIC_WRITE|GENERIC_READ, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
		if(INVALID_HANDLE_VALUE == file_)
		{
			return false;
		}
		SetFilePointer(file_, 0, NULL, FILE_BEGIN);
		
		return true;
	}

	void Uninit()
	{
		AUTOUniLock(lock_);
		if(INVALID_HANDLE_VALUE != file_)
		{
			CloseHandle(file_);
			file_ = INVALID_HANDLE_VALUE;
		}
	}

	void SetLevel(LOG_LEVEL level)
	{
		level_ = level;
	}
	void Error(const char *format, ...)
	{
		if(LL_Err > level_)
			return ;

		if(INVALID_HANDLE_VALUE == file_)
			return ;

		va_list args;
		va_start(args, format);

		Output(LL_Err, format, args);

		va_end(args);
	}
	void Warn(const char *format, ...)
	{
		if(LL_Warning > level_)
			return ;

		if(INVALID_HANDLE_VALUE == file_)
			return ;

		va_list args;
		va_start(args, format);

		Output(LL_Warning, format, args);

		va_end(args);
	}
	void Info(const char *format, ...)
	{
		if(LL_Info > level_)
			return ;

		if(INVALID_HANDLE_VALUE == file_)
			return ;

		va_list args;
		va_start(args, format);

		Output(LL_Info, format, args);

		va_end(args);
	}
	void Debug(const char *format, ...)
	{
		if(LL_Debug > level_)
			return ;

		if(INVALID_HANDLE_VALUE == file_)
			return ;

		va_list args;
		va_start(args, format);

		Output(LL_Debug, format, args);

		va_end(args);
	}

	void Binary(unsigned char* buf, unsigned int len)
	{
		if(INVALID_HANDLE_VALUE == file_)
			return ;

		AUTOUniLock(lock_);
		DWORD wrote = 0;
		WriteFile(file_, buf, len, &wrote, NULL);
	}

private:
	FileLog(void)
		:file_(INVALID_HANDLE_VALUE)
		,current_Processid_(0)
		,level_(LL_Info)
	{

	}
	~FileLog()
	{

	}

private:
	void Output(LOG_LEVEL level, const char *format, va_list args)
	{
		char szPrefix[20];
		switch(level)
		{
		case LL_Err:
			strcpy_s(szPrefix, 20, "错误-");
			break;
		case LL_Warning:        
			strcpy_s(szPrefix, 20, "警告-");
			break;
		case LL_Info:        
			strcpy_s(szPrefix, 20, "信息-");
			break;
		case LL_Debug:        
			strcpy_s(szPrefix, 20, "调试-");
			break;
		default:
			return ;
		}

		SYSTEMTIME time;
		GetLocalTime(&time);
		char szBuffer[1024];
		sprintf_s( szBuffer,"\r\n[%04d/%d]%02d%02d %02d:%02d:%02d.%03d |%s",
			current_Processid_,GetCurrentThreadId(),
			time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, szPrefix);
		size_t len = strlen(szBuffer);
		int nBuf = _vsnprintf_s(szBuffer+len, 1024-len, _TRUNCATE, format, args);

		AUTOUniLock(lock_);
		if(INVALID_HANDLE_VALUE != file_)
		{
			DWORD dwWrote = 0;
			len = strlen(szBuffer);
			WriteFile(file_, szBuffer, len, &dwWrote, NULL );
		}
	}

private:
	HANDLE file_;
	util::UniLock lock_;
	DWORD current_Processid_;
	LOG_LEVEL level_;
};

}

#endif
