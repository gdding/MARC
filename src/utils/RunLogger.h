/*-----------------------------------------------------------------------------
* Copyright (c) 2010 ICT, CAS. All rights reserved.
*   dingguodong@software.ict.ac.cn
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.

* Last-Modified: 2010-02-04
*-----------------------------------------------------------------------------*/
#ifndef _H_RUNLOGGER_GDDING_INCLUDED_20090603
#define _H_RUNLOGGER_GDDING_INCLUDED_20090603
#include "StdHeader.h"

//��־�ص�������ÿ��д��һ����־����ã�
typedef bool (*logcb)(const char* log, void* arg);

class CRunLogger
{
public:
	CRunLogger(const char* sLogFile, bool bStdOut=false);
	virtual ~CRunLogger();

public:
	typedef enum
	{
		LOG_INFO		= 0, //��ͨ��ʾ��־
		LOG_WARNING		= 1, //������־
		LOG_ERROR		= 2  //������־
	}LogType;
	void Write(LogType t, const char* sLog, ...);

	//�����쳣��־�ص�����
	void SetErrLogCallback(logcb func, void* arg);

public:
	void Dump2Html(string &html);
	void DumpErr2Html(string &html);

private:
	FILE*	m_fpLogFile;
	int		m_nFileDate;
	string	m_sLogFileName;
	bool	m_bStdOut; //�Ƿ��������Ļ
	list<pair<LogType,string> > m_oLogs;
	list<pair<LogType,string> > m_oErrLogs;
	DEFINE_LOCKER(m_locker);

private:
	logcb m_pfLogCallback;
	void* m_pArgPrivate;
};

#endif //_H_RUNLOGGER_GDDING_INCLUDED_20090603
