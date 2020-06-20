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

#define APP_EXIT_CODE_TIMEOUT 101  //��ʱ�˳�
#define APP_EXIT_CODE_ABORT     110  //�쳣�˳�


//������������ʹ��fork���ã�
class CAppRunner
{
public:
	CAppRunner();
	~CAppRunner();

public:
	//ͨ��fork����ִ��Ӧ�ó���
	bool ExecuteApp(const char* sAppCmd);

	//����ɱ���������е�Ӧ�ó���
	void KillApp();

	//Ӧ�ó����Ƿ���������
	bool IsAppRunning();

	//��ó������н������˳���
	int GetAppExitCode();

private:
	CLoopThread* m_pWorkThread;
	static void WorkRutine(void* param);

private:
	string m_sAppCmd;      //�ý��̶�Ӧ�ĳ�������
	DEFINE_LOCKER(m_locker);

#ifdef WIN32
	HANDLE  m_hProcess;     //���̾��
	DWORD  m_nExitCode;
#else
	int           m_hProcess;           //����ID
	int           m_nExitCode;
#endif
};


#endif //_H_APPRUNNER_GDDING_INCLUDED_20100130
