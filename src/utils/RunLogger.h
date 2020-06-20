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

//日志回调函数（每当写完一条日志后调用）
typedef bool (*logcb)(const char* log, void* arg);

class CRunLogger
{
public:
	CRunLogger(const char* sLogFile, bool bStdOut=false);
	virtual ~CRunLogger();

public:
	typedef enum
	{
		LOG_INFO		= 0, //普通提示日志
		LOG_WARNING		= 1, //警告日志
		LOG_ERROR		= 2  //错误日志
	}LogType;
	void Write(LogType t, const char* sLog, ...);

	//设置异常日志回调函数
	void SetErrLogCallback(logcb func, void* arg);

public:
	void Dump2Html(string &html);
	void DumpErr2Html(string &html);

private:
	FILE*	m_fpLogFile;
	int		m_nFileDate;
	string	m_sLogFileName;
	bool	m_bStdOut; //是否输出到屏幕
	list<pair<LogType,string> > m_oLogs;
	list<pair<LogType,string> > m_oErrLogs;
	DEFINE_LOCKER(m_locker);

private:
	logcb m_pfLogCallback;
	void* m_pArgPrivate;
};

#endif //_H_RUNLOGGER_GDDING_INCLUDED_20090603
