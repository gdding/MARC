#include "RunLogger.h"
#include "Utility.h"


CRunLogger::CRunLogger(const char* sLogFile,bool bStdOut)
{
	m_pfLogCallback = NULL;
	m_pArgPrivate = NULL;
	INITIALIZE_LOCKER(m_locker);
	LOCK(m_locker);
	m_sLogFileName = sLogFile;
	CreateFilePath(sLogFile);
	m_nFileDate = getCurDate();;
	m_fpLogFile = NULL;
	UNLOCK(m_locker);
	m_bStdOut = bStdOut;
}

CRunLogger::~CRunLogger()
{
	LOCK(m_locker);
	if(m_fpLogFile != NULL)
	{
		fclose(m_fpLogFile);
	}
	UNLOCK(m_locker);
	DESTROY_LOCKER(m_locker);
}

void CRunLogger::Write(LogType t, const char* sLog, ...)
{
	string sLogType = "Info";
	if(t == this->LOG_WARNING) sLogType = "Warning";
	if(t == this->LOG_ERROR) sLogType = "Error";

	char header[100] = {0};
	string sDateTime = formatDateTime(time(NULL));
	sprintf(header, "<%s> %s: ", sDateTime.c_str(), sLogType.c_str());
	string sFullLog = header;

	va_list vl;
	va_start(vl, sLog);
	char buf[1025] = {0};
	_vsnprintf(buf, 1024, sLog, vl);
	sFullLog += buf;
	va_end(vl);

	LOCK(m_locker);
	if(t != this->LOG_INFO && m_pfLogCallback != NULL)
	{
		m_pfLogCallback(sFullLog.c_str(), m_pArgPrivate);
	}
	m_oLogs.push_back(make_pair(t, sFullLog));
	while(m_oLogs.size() > 100)
	{
		m_oLogs.pop_front();
	}
	if(t != this->LOG_INFO)
	{
		m_oErrLogs.push_back(make_pair(t, sFullLog));
		while(m_oErrLogs.size() > 100)
		{
			m_oErrLogs.pop_front();
		}
	}

	if(m_fpLogFile == NULL)
	{
		char chLogFile[512] = {0};
		sprintf(chLogFile, "%s.%d", m_sLogFileName.c_str(), m_nFileDate);
		m_fpLogFile = fopen(chLogFile, "at");
		if(m_fpLogFile != NULL)
			fprintf(m_fpLogFile, "\n################################\n");
	}
	
	if(m_fpLogFile != NULL)
	{
		if(m_nFileDate != getCurDate())
		{
			m_nFileDate = getCurDate();
			char chLogFile[512] = {0};
			sprintf(chLogFile, "%s.%d", m_sLogFileName.c_str(), m_nFileDate);
			fclose(m_fpLogFile);
			m_fpLogFile = fopen(chLogFile, "at");
		}

		fprintf(m_fpLogFile, "%s", sFullLog.c_str());
		fflush(m_fpLogFile);
	}
	if(m_bStdOut)
	{
		fprintf(stdout, "%s", sFullLog.c_str());
		fflush(stdout);
	}

	UNLOCK(m_locker);
}

void CRunLogger::SetErrLogCallback(logcb func, void* arg)
{
	this->m_pfLogCallback = func;
	this->m_pArgPrivate = arg;
}

void CRunLogger::Dump2Html(string &html)
{
	html = "<html><head></head><body style=\"font-size:14px\">";
	LOCK(m_locker);
	list<pair<LogType,string> >::const_iterator it;
	for(it = m_oLogs.begin(); it != m_oLogs.end(); ++it)
	{
		char buf[1024] = {0};
		string color = "";
		if(it->first == this->LOG_WARNING) color = "#CC00CC";
		if(it->first == this->LOG_ERROR) color = "#FF0000";
		_snprintf(buf, sizeof(buf), "<li style=\"padding-top: 2px; padding-bottom: 2px\"><font color=\"%s\">%s</font></li>\n", color.c_str(), it->second.c_str());
		html += buf;
	}
	UNLOCK(m_locker);
	html += "</table></div></body></html>";
}

void CRunLogger::DumpErr2Html(string &html)
{
	html = "<html><head></head><body style=\"font-size:14px\">";
	LOCK(m_locker);
	if(m_oErrLogs.empty())
	{
		html += "<font size=6>恭喜你！节点启动后没有异常日志。</font>";
	}
	else
	{
		list<pair<LogType,string> >::const_iterator it;
		for(it = m_oErrLogs.begin(); it != m_oErrLogs.end(); ++it)
		{
			char buf[1024] = {0};
			string color = "";
			if(it->first == this->LOG_WARNING) color = "#CC00CC";
			if(it->first == this->LOG_ERROR) color = "#FF0000";
			_snprintf(buf, sizeof(buf), "<li style=\"padding-top: 2px; padding-bottom: 2px\"><font color=\"%s\">%s</font></li>\n", color.c_str(), it->second.c_str());
			html += buf;
		}
	}
	UNLOCK(m_locker);
	html += "</table></div></body></html>";
}

