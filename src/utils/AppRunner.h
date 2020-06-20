/*-----------------------------------------------------------------------------
* Copyright (c) 2010 ICT, CAS. All rights reserved.
*   dingguodong@software.ict.ac.cn
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.

* Last-Modified: 2010-02-04
*-----------------------------------------------------------------------------*/
#ifndef _H_APPRUNNER_GDDING_INCLUDED_20100130
#define _H_APPRUNNER_GDDING_INCLUDED_20100130
#include "StdHeader.h"
class CLoopThread;

#define APP_EXIT_CODE_TIMEOUT 101  //超时退出
#define APP_EXIT_CODE_ABORT     110  //异常退出


//程序运行器（使用fork调用）
class CAppRunner
{
public:
	CAppRunner();
	~CAppRunner();

public:
	//通过fork调用执行应用程序
	bool ExecuteApp(const char* sAppCmd);

	//主动杀死正在运行的应用程序
	void KillApp();

	//应用程序是否正在运行
	bool IsAppRunning();

	//获得程序运行结束的退出码
	int GetAppExitCode();

private:
	CLoopThread* m_pWorkThread;
	static void WorkRutine(void* param);

private:
	string m_sAppCmd;      //该进程对应的程序命令
	DEFINE_LOCKER(m_locker);

#ifdef WIN32
	HANDLE  m_hProcess;     //进程句柄
	DWORD  m_nExitCode;
#else
	int           m_hProcess;           //进程ID
	int           m_nExitCode;
#endif
};


#endif //_H_APPRUNNER_GDDING_INCLUDED_20100130
