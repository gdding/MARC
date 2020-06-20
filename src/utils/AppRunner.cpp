#include "AppRunner.h"
#include "LoopThread.h"
#include "Utility.h"

CAppRunner::CAppRunner()
{
	m_sAppCmd = "";
	m_hProcess = 0;
	m_nExitCode = 0;
	INITIALIZE_LOCKER(m_locker);
}

CAppRunner::~CAppRunner()
{
	if(m_hProcess != 0)
	{
#ifdef WIN32
		TerminateProcess(m_hProcess, -1);
		CloseHandle(m_hProcess);
#else
		int status = 0;
		kill(m_hProcess, SIGTERM);
		waitpid(m_hProcess, &status, 0);
#endif
	}
	DESTROY_LOCKER(m_locker);
}

bool CAppRunner::ExecuteApp(const char* sAppCmd)
{
	//子进程还在运行时，不允许再次执行
	if(m_hProcess != 0) return false;
	m_sAppCmd = sAppCmd;

#ifdef WIN32
	vector<string> args;
	SplitCmdStringBySpace(sAppCmd, args);
	string sParameter("");
	for(size_t i = 1; i < args.size(); i++)
	{
		sParameter += args[i] + " ";
	}

	//使用ShellExecuteEx启动进程
	SHELLEXECUTEINFO shExecInfo;
	memset(&shExecInfo, 0, sizeof(SHELLEXECUTEINFO));
	shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	//shExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
	shExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	shExecInfo.hwnd = NULL;
	shExecInfo.lpVerb = NULL;
	shExecInfo.lpFile = args[0].c_str();
	shExecInfo.lpParameters = sParameter.c_str();
	shExecInfo.lpDirectory = NULL;
	//shExecInfo.nShow = SW_SHOW;
	shExecInfo.hInstApp = NULL;
	if(ShellExecuteEx(&shExecInfo))
	{
		LOCK(m_locker);
		m_hProcess = shExecInfo.hProcess;
		m_nExitCode = 0;
		UNLOCK(m_locker);
		return true;
	}
	return false;
#else //linux
	vector<string> args;
	char* exec_argv[257] = {0};
	SplitCmdStringBySpace(sAppCmd, args);
	size_t i = 0;
	for(i = 0; i < args.size() && i < 256; i++)
	{
		exec_argv[i] = (char*)args[i].c_str();
	}
	exec_argv[i] = NULL;
	pid_t pid;
	switch(pid=fork())
	{
	case (pid_t)-1:
		return false;
	case (pid_t)0: //子进程
		::execv(exec_argv[0], exec_argv);
		//printf("error: %s\n", strerror(errno));
		_exit(APP_EXIT_CODE_ABORT);
	default: //父进程
		LOCK(m_locker);
		m_hProcess = pid;
		m_nExitCode = 0;
		UNLOCK(m_locker);
		return true;
	};
#endif
}

bool CAppRunner::IsAppRunning()
{
	if(m_hProcess == 0) 
		return false;

	LOCK(m_locker);
	//探测并更新子进程运行状态
#ifdef WIN32
	GetExitCodeProcess(m_hProcess, &m_nExitCode);
	if(m_nExitCode != STILL_ACTIVE)
	{
		CloseHandle(m_hProcess);
		m_hProcess = 0;
	}
#else
	int status = 0;
	pid_t pid = waitpid(m_hProcess, &status, WNOHANG);
	if(pid > 0) //status changed
	{
		m_hProcess = 0;
		m_nExitCode = (WIFEXITED(status)?WEXITSTATUS(status):APP_EXIT_CODE_ABORT);
	}
	else if(pid < 0) //发生错误
	{
		kill(m_hProcess, SIGTERM);
		waitpid(m_hProcess, &status, 0);
		m_hProcess = 0;
		m_nExitCode = APP_EXIT_CODE_ABORT;
	}
#endif
	UNLOCK(m_locker);
	return m_hProcess!=0;
}

void CAppRunner::KillApp()
{
	LOCK(m_locker);
	if(m_hProcess != 0)
	{
#ifdef WIN32
		TerminateProcess(m_hProcess, -1);
		CloseHandle(m_hProcess);
#else
		int status = 0;
		kill(m_hProcess, SIGTERM);
		waitpid(m_hProcess, &status, 0);
#endif 
		//usleep(10000);
		m_nExitCode = APP_EXIT_CODE_ABORT;
		m_hProcess = 0;
	}
	UNLOCK(m_locker);
}

int CAppRunner::GetAppExitCode()
{
	return m_nExitCode;
}
