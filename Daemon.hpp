#ifndef Daemon_hpp_
#define Daemon_hpp_

//#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdio.h> 
#include <psapi.h>

#include <string>

#include "StrTool.hpp"
#include "SimpleFileLog.hpp"

#pragma comment (lib,"Psapi.lib")

namespace util
{

/** 
 * @brief 自动释放句柄2abcdefgbb
 * @author liuheng
 * 
 * 自动释放句柄
 */
class AutoCloseHandler{
public:
	AutoCloseHandler(HANDLE h = NULL)
		: h_(h)
	{
	}
	~AutoCloseHandler()
	{
		Close();
	}

	void Reset(HANDLE h)
	{
		Close();
		h_ = h;
	}

	operator HANDLE()
	{
		return h_;
	}

private:
	void Close()
	{
		if (h_ != NULL)
		{
			CloseHandle(h_);
			h_ = NULL;
		}
	}

private:
	HANDLE h_;
};

//////////////////////////////////////////////////////////////////////////
static bool GetExeName(char* strExeName)
{
	//获取当前进程的ID
	DWORD processID = ::GetCurrentProcessId();
	//通过进程ID获取到进程的句柄
	AutoCloseHandler hProcess(OpenProcess( PROCESS_QUERY_INFORMATION |
		PROCESS_VM_READ,
		FALSE, processID ));
	if (NULL != hProcess )
	{
		HMODULE hMod;
		DWORD cbNeeded;
		//获取当前模块的句柄
		if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), 
			&cbNeeded) )
		{
			//获取进程的绝对路径
			GetModuleFileNameExA(hProcess, hMod, strExeName, MAX_PATH);
			if(strExeName[0] == '\0')
			{
				return false;
			}
		}
	}
	return true;
}

static bool RunExename(char* exe, HANDLE& phandler) 
{ 
	STARTUPINFOA si = { sizeof(si)}; 
	PROCESS_INFORMATION pi; 

	si.dwFlags = STARTF_USESHOWWINDOW; 
	si.wShowWindow = TRUE; //TRUE表示显示创建的进程的窗口
	BOOL bRet = ::CreateProcessA ( 
		NULL,
		exe, 
		NULL, 
		NULL, 
		FALSE, 
		CREATE_NEW_CONSOLE, 
		NULL, 
		NULL, 
		&si, 
		&pi); 

	if(bRet) 
	{ 
		phandler = pi.hProcess;

		::CloseHandle (pi.hThread); 
		//::CloseHandle (pi.hProcess); 
	} 

	return bRet; 
} 
//////////////////////////////////////////////////////////////////////////

/** 
 * @brief 守护类
 * @author liuheng
 * @param[in] arg0 desc
 * @param[in] arg1 desc
 * @retval true desc
 * @return desc
 * 
 */
class Daemon{
public:
	Daemon(const char* cmdline)
		:is_daemon_(true)
		,my_pid_(0)
		,start_time_(0)
		,daemon_thread_(NULL)
		,last_restarthour_(0)
		,last_restartday_(0)
	{
		memset(exe_, 0, MAX_PATH*sizeof(char));
		memset(argv_, 0, MAX_PATH*sizeof(char));

		const char* firstspace = strstr(cmdline, " ");
		if (firstspace != NULL)
		{
			strcat(argv_, firstspace);
		}
		if (strstr(cmdline, "_notdaemon_") != NULL)
		{
			is_daemon_ = false;
		}

		GetExeName(exe_);
		my_pid_ = GetCurrentProcessId();
	}

	~Daemon()
	{
	}

	void start(const std::string& restart_hours){
		if (is_daemon_ == true)
		{
			util::FileLog::Ins().Init();

			util::FileLog::Ins().Info("守护进程[%d]启动，守护规则:%s", my_pid_, restart_hours.c_str());

			// 守护进程
			util::Str2Set(restart_hours, restart_hours_, ',');
			daemon_thread_ = CreateThread(NULL, 0, DaemonThread, this, 0, NULL);	

			WaitForSingleObject(daemon_thread_, INFINITE);
		}
	}

protected:
	static DWORD WINAPI DaemonThread(PVOID pParam)
	{
		Daemon* pThis = (Daemon*)pParam;

		return pThis->DaemonFunc();
	}

	DWORD DaemonFunc()
	{
		// daemon
		while (true)
		{
			util::FileLog::Ins().Info("守护进程[%d]，检测一次执行进程", my_pid_);

			bool isexist = false;
			if(ach_ != NULL)
			{
				isexist = true;
				DWORD ec = WaitForSingleObject(ach_, 60*1000);
				if (ec == WAIT_OBJECT_0)
				{
					util::FileLog::Ins().Info("###检测到执行进程退出了");
					ach_.Reset(NULL);
					isexist = false;
				}
			}else
			{
				util::FileLog::Ins().Info("###检测到执行进程不存在了");
			}

			SYSTEMTIME st;
			GetLocalTime(&st);
			int curday = st.wDay;
			int curhour = st.wHour;

			if (isexist)
			{
				// exist
				util::FileLog::Ins().Info("~~~找到执行进程，判断是否需要重启");
				char strhour[10];
				sprintf_s(strhour, "%d", curhour);
				if (restart_hours_.find(strhour)!=restart_hours_.end())
				{
					if (last_restarthour_!=curhour || last_restartday_!=curday)
					{
						// kill process
						util::FileLog::Ins().Info("~~~需要重新启动执行进程了");
						stop_child();
						Sleep(2000);
					}
				}
			}else{
				// not exist
				util::FileLog::Ins().Info("###没有找到执行进程，创建一个");
				if(start_child()){
					last_restartday_ = curday;
					last_restarthour_ = curhour;
				}
			}
		}

		return 0;
	}

private:
	bool stop_child(){
		char cmdline[MAX_PATH*2] = {0};

		if(ach_ != NULL)
		{
			bool ret = TerminateProcess(ach_, 0);
			util::FileLog::Ins().Info("结束进程失败[%d][%d]", ret, GetLastError());
		}
		return true;
	}

	bool start_child(){
		char cmdline[MAX_PATH*2] = {0};

		strcpy(cmdline, exe_);
		strcat(cmdline, argv_);
		strcat(cmdline, " _notdaemon_");

		HANDLE phandler = NULL;
		bool ret = RunExename(cmdline, phandler);
		if (ret)
		{
			ach_.Reset(phandler);
		}

		start_time_ = time(NULL);
		return ret;
	}

private:
	bool is_daemon_;

	// 当前进程
	int my_pid_;
	
	// 被守护的进程
	AutoCloseHandler ach_;

	// 启动信息
	char exe_[MAX_PATH];
	char argv_[MAX_PATH];

	// 时间
	int start_time_;
	int last_restartday_;
	int last_restarthour_;
	std::set<std::string> restart_hours_;
	HANDLE daemon_thread_;
};

}

#endif