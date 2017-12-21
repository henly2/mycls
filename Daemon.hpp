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
 * @brief �Զ��ͷž��2abcdefgbb
 * @author liuheng
 * 
 * �Զ��ͷž��
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
	//��ȡ��ǰ���̵�ID
	DWORD processID = ::GetCurrentProcessId();
	//ͨ������ID��ȡ�����̵ľ��
	AutoCloseHandler hProcess(OpenProcess( PROCESS_QUERY_INFORMATION |
		PROCESS_VM_READ,
		FALSE, processID ));
	if (NULL != hProcess )
	{
		HMODULE hMod;
		DWORD cbNeeded;
		//��ȡ��ǰģ��ľ��
		if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), 
			&cbNeeded) )
		{
			//��ȡ���̵ľ���·��
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
	si.wShowWindow = TRUE; //TRUE��ʾ��ʾ�����Ľ��̵Ĵ���
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
 * @brief �ػ���
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

			util::FileLog::Ins().Info("�ػ�����[%d]�������ػ�����:%s", my_pid_, restart_hours.c_str());

			// �ػ�����
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
			util::FileLog::Ins().Info("�ػ�����[%d]�����һ��ִ�н���", my_pid_);

			bool isexist = false;
			if(ach_ != NULL)
			{
				isexist = true;
				DWORD ec = WaitForSingleObject(ach_, 60*1000);
				if (ec == WAIT_OBJECT_0)
				{
					util::FileLog::Ins().Info("###��⵽ִ�н����˳���");
					ach_.Reset(NULL);
					isexist = false;
				}
			}else
			{
				util::FileLog::Ins().Info("###��⵽ִ�н��̲�������");
			}

			SYSTEMTIME st;
			GetLocalTime(&st);
			int curday = st.wDay;
			int curhour = st.wHour;

			if (isexist)
			{
				// exist
				util::FileLog::Ins().Info("~~~�ҵ�ִ�н��̣��ж��Ƿ���Ҫ����");
				char strhour[10];
				sprintf_s(strhour, "%d", curhour);
				if (restart_hours_.find(strhour)!=restart_hours_.end())
				{
					if (last_restarthour_!=curhour || last_restartday_!=curday)
					{
						// kill process
						util::FileLog::Ins().Info("~~~��Ҫ��������ִ�н�����");
						stop_child();
						Sleep(2000);
					}
				}
			}else{
				// not exist
				util::FileLog::Ins().Info("###û���ҵ�ִ�н��̣�����һ��");
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
			util::FileLog::Ins().Info("��������ʧ��[%d][%d]", ret, GetLastError());
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

	// ��ǰ����
	int my_pid_;
	
	// ���ػ��Ľ���
	AutoCloseHandler ach_;

	// ������Ϣ
	char exe_[MAX_PATH];
	char argv_[MAX_PATH];

	// ʱ��
	int start_time_;
	int last_restartday_;
	int last_restarthour_;
	std::set<std::string> restart_hours_;
	HANDLE daemon_thread_;
};

}

#endif