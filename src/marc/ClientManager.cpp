#include "ClientManager.h"
#include "TaskManager.h"
#include "../utils/LoopThread.h"
#include "../utils/Utility.h"
#include "../utils/DirScanner.h"
#include "../utils/RunLogger.h"


CClientManager::CClientManager(CTaskManager* pTaskManager, CMasterConf* pConfigure, CRunLogger* pLogger)
{
	m_pTaskManager = pTaskManager;
	m_pConfigure = pConfigure;
	m_pLogger = pLogger;
	INITIALIZE_LOCKER(m_locker);

	m_nLastWatchTime = 0;
	m_pVerWatchThread = new CLoopThread();
	assert(m_pVerWatchThread != NULL);
	m_pVerWatchThread->SetRutine(VerWatchRutine, this);
	m_pVerWatchThread->Start();
}

CClientManager::~CClientManager()
{
	m_pVerWatchThread->Stop();
	delete m_pVerWatchThread;

	LOCK(m_locker);
	map<int, CClientInfo*>::iterator  jt = m_oClients.begin();
	for(; jt != m_oClients.begin(); ++jt)
	{
		if(jt->second != NULL)
			delete jt->second;
	}
	m_oClients.clear();
	UNLOCK(m_locker);

	DESTROY_LOCKER(m_locker);
}

bool CClientManager::AddClient(int nClientID, const string& sAppType, const string& sIP, const string& sInstallPath)
{
	LOCK(m_locker);
	CClientInfo* pClient = NULL;
	map<int, CClientInfo*>::iterator it = m_oClients.find(nClientID);
	if(it == m_oClients.end())
	{
		pClient = new CClientInfo();
		assert(pClient != NULL);
		m_oClients[nClientID] = pClient;
	}
	else
	{
		pClient = it->second;
		assert(pClient->bDisabled);
		assert(pClient->nClientID == nClientID);
	}
	assert(pClient != NULL);
	pClient->nClientID = nClientID;
	pClient->sAppType = sAppType;
	pClient->sIP = sIP;
	pClient->sInstallPath = sInstallPath;
	pClient->nRegisterTime = time(0);
	pClient->bTaskRequested = false;
	pClient->nLastActiveTime = time(0);
	memset(&pClient->dAppState, 0, sizeof(_stClientState));
	memset(&pClient->dNodeStatus, 0, sizeof(_stClientStatus));
	memset(&pClient->dSourceStatus, 0, sizeof(_stNodeSourceStatus));
	pClient->dErrLogs.clear();
	pClient->bDisabled = false;
	pClient->nDisabledTime = 0;

	//��ĳ����ʧЧ��Client�ڵ��IP��InstallPath�������ڵ���ͬ��ɾ��֮
	it = m_oClients.begin();
	for(; it != m_oClients.end(); )
	{
		CClientInfo* pClient = it->second;
		assert(pClient != NULL);
		if(pClient->bDisabled && pClient->sIP == sIP && pClient->sInstallPath==sInstallPath)
		{
			m_pLogger->Write(CRunLogger::LOG_INFO, "Delete disabled ClientNode(ID=%d, AppType=%s, IP=%s)! %s:%d\n", 
				pClient->nClientID, pClient->sAppType.c_str(), pClient->sIP.c_str(), __FILE__, __LINE__);
			m_oClients.erase(it++);
			delete pClient;
		}
		else
		{
			++it;
		}
	}
	UNLOCK(m_locker);

	LOCK(m_pConfigure->locker4ClientApp);
	if(m_pConfigure->oClientAppVersion.find(sAppType) == m_pConfigure->oClientAppVersion.end())
	{
		CAppUpdateInfo* p = new CAppUpdateInfo();
		assert(p != NULL);
		p->sAppType = sAppType;
		p->nAppUpdateVer = 0;
		p->sAppUpdateFile = "";
		m_pConfigure->oClientAppVersion[sAppType] = p;

		//��������AppTypeɨ���Ƿ���������
		int nAppUpdateVersion = 0;
		string sAppUpdateZipFile = "";
		if(ScanClientAppUpdateDir(sAppType, 0, nAppUpdateVersion, sAppUpdateZipFile))
		{
			p->nAppUpdateVer = nAppUpdateVersion;
			p->sAppUpdateFile = sAppUpdateZipFile;
		}
	}
	UNLOCK(m_pConfigure->locker4ClientApp);

	LOCK(m_pConfigure->locker4ResultApp);
	if(m_pConfigure->oResultAppVersion.find(sAppType) == m_pConfigure->oResultAppVersion.end())
	{
		CAppUpdateInfo* p = new CAppUpdateInfo();
		assert(p != NULL);
		p->sAppType = sAppType;
		p->nAppUpdateVer = 0;
		p->sAppUpdateFile = "";
		m_pConfigure->oResultAppVersion[sAppType] = p;
	}
	UNLOCK(m_pConfigure->locker4ResultApp);

	return true;
}

bool CClientManager::FindClient(int nClientID, bool &bDisabled)
{
	bool bFound = false;
	bDisabled = false;
	
	LOCK(m_locker);
	map<int, CClientInfo*>::iterator it = m_oClients.find(nClientID);
	if( it != m_oClients.end())
	{
		bDisabled = it->second->bDisabled;
		bFound = true;
	}
	UNLOCK(m_locker);

	return bFound;
}

bool CClientManager::FindClient(const string& sIP, const string& sInstallPath, int &nClientID, bool &bDisabled)
{
	bool bFound = false;
	nClientID = 0;
	bDisabled = false;

	LOCK(m_locker);
	map<int, CClientInfo*>::iterator it = m_oClients.begin();
	for(; it != m_oClients.end(); it++)
	{
		if(sIP == it->second->sIP  && sInstallPath == it->second->sInstallPath)
		{
			nClientID = it->first;
			bDisabled = it->second->bDisabled;
			bFound = true;
			break;
		}
	}
	UNLOCK(m_locker);
	return bFound;
}

bool CClientManager::SetActiveTime(int nClientID, time_t t)
{
	bool ret = false;
	LOCK(m_locker);
	map<int, CClientInfo*>::iterator mit = m_oClients.find(nClientID);
	if(mit != m_oClients.end())
	{
		CClientInfo* pClient = mit->second;
		assert(pClient != NULL);
		pClient->nLastActiveTime = t;
		ret = true;
	}
	UNLOCK(m_locker);
	return ret;
}

bool CClientManager::SetTaskRequested(int nClientID, bool bRequested)
{
	bool ret = false;
	LOCK(m_locker);
	map<int, CClientInfo*>::iterator mit = m_oClients.find(nClientID);
	if(mit != m_oClients.end())
	{
		CClientInfo* pClient = mit->second;
		assert(pClient != NULL);
		pClient->bTaskRequested = bRequested;
		ret = true;
	}
	UNLOCK(m_locker);
	return ret;
}

bool CClientManager::SetRunningStatus(int nClientID, const _stClientStatus* pNodeStatus)
{
	bool ret = false;
	LOCK(m_locker);
	map<int, CClientInfo*>::iterator mit = m_oClients.find(nClientID);
	if(mit != m_oClients.end())
	{
		CClientInfo* pClient = mit->second;
		assert(pClient != NULL);
		memcpy(&pClient->dNodeStatus, pNodeStatus, sizeof(_stClientStatus));
		ret = true;
	}
	UNLOCK(m_locker);
	return ret;
}

bool CClientManager::SetSourceStatus(int nClientID, const _stNodeSourceStatus* pSourceStatus)
{
	bool ret = false;
	LOCK(m_locker);
	map<int, CClientInfo*>::iterator mit = m_oClients.find(nClientID);
	if(mit != m_oClients.end())
	{
		CClientInfo* pClient = mit->second;
		assert(pClient != NULL);
		memcpy(&pClient->dSourceStatus, pSourceStatus, sizeof(_stNodeSourceStatus));
		ret = true;
	}
	UNLOCK(m_locker);
	return ret;
}

bool CClientManager::AddErrLog(int nClientID, const char* sErrLog)
{
	bool ret = false;
	LOCK(m_locker);
	map<int, CClientInfo*>::iterator mit = m_oClients.find(nClientID);
	if(mit != m_oClients.end())
	{
		CClientInfo* pClient = mit->second;
		assert(pClient != NULL);
		assert(sErrLog != NULL);
		pClient->dErrLogs.push_back(sErrLog);
		while(pClient->dErrLogs.size() > MARC_MAX_KEEP_LOGINFO)
		{
			pClient->dErrLogs.pop_front();
		}
		ret = true;
	}
	UNLOCK(m_locker);
	return ret;
}

int CClientManager::NodeCount()
{
	LOCK(m_locker);
	int nClientCount =  (int)m_oClients.size();
	UNLOCK(m_locker);
	return nClientCount;
}

void CClientManager::GetClientsOfNeedCreateTask(vector<int>& oClientIDs, vector<string>& oClientAppTypes)
{
	typedef vector<pair<int,string> > TClientIDAppType;
	TClientIDAppType oClients;

	LOCK(m_locker);
	map<int, CClientInfo*>::iterator it = m_oClients.begin();
	for(; it != m_oClients.end(); ++it)
	{
		int nClientID = it->first;
		CClientInfo* pClient = it->second;
		assert(pClient != NULL);
		if(pClient->bDisabled) continue;
		if(m_pConfigure->nTaskCreateStrategy == MARC_TASKCREATE_ONLYWHEN_REQUESTED && !pClient->bTaskRequested) continue;
		oClients.push_back(make_pair(nClientID, pClient->sAppType));
	}
	UNLOCK(m_locker);

	for(size_t i=0; i < oClients.size(); i++)
	{
		int nClientID = oClients[i].first;
		const string& sAppType = oClients[i].second;
		if(!m_pTaskManager->FindTask(nClientID, sAppType))
		{
			oClientIDs.push_back(nClientID);
			oClientAppTypes.push_back(sAppType);
		}
	}
}

bool CClientManager::RemoveClient(int nClientID)
{
	bool bRemoved = false;
	string sAppType(""), sIP("");

	LOCK(m_locker);
	map<int, CClientInfo*>::iterator mit = m_oClients.find(nClientID);
	if(mit != m_oClients.end())
	{
		CClientInfo* pClient = mit->second;
		if(!pClient->bDisabled)
		{
			m_pLogger->Write(CRunLogger::LOG_WARNING, "Set ClientNode(ID=%d, AppType=%s, IP=%s) to be disabled! %s:%d\n", 
				nClientID, pClient->sAppType.c_str(), pClient->sIP.c_str(), __FILE__, __LINE__);

			pClient->bDisabled = true;
			pClient->nDisabledTime = time(0);
			bRemoved = true;
		}
	}
	UNLOCK(m_locker);

	if(bRemoved)
	{
		//��Client�ڵ������δ��ɵ�������Ϊִ��ʧ��
		m_pTaskManager->SetTaskFailed(nClientID);
	}

	return bRemoved;
}

bool CClientManager::SetTaskID(int nClientID, int nTaskID)
{
	bool ret = false;
	LOCK(m_locker);
	map<int, CClientInfo*>::iterator it = m_oClients.find(nClientID);
	if(it != m_oClients.end())
	{
		it->second->nCurTaskID = nTaskID;
		ret = true;
	}
	UNLOCK(m_locker);
	return ret;
}

bool CClientManager::UpdateClientState(int nClientID, const _stClientState& dAppState)
{
	bool ret = false;
	LOCK(m_locker);
	map<int, CClientInfo*>::iterator it = m_oClients.find(nClientID);
	if(it != m_oClients.end())
	{
		CClientInfo* pClientInfo = it->second;
		assert(pClientInfo != NULL);
		memcpy(&pClientInfo->dAppState, &dAppState, sizeof(_stClientState));
		pClientInfo->tLastUpdateTime = time(0);
		ret = true;
	}
	UNLOCK(m_locker);
	return ret;
}

void CClientManager::SaveClientState(const string& sSaveTime, vector<string>& oStateFiles)
{
#if 0
	/***
	* ��ÿ���ڵ�ش�������״̬��Ϣ���浽һ����Ӧ��״̬�ļ��У�
	* ״̬�ļ��������淶��[CreateTime].[ClientID]����201002041348.2
	* ͬʱ���ѱ�����ɵ�״̬�ļ�������oStateFiles�С�
	****/
	LOCK(m_locker);
	map<int, CClientInfo*>::iterator it = m_oClients.begin();
	for(; it != m_oClients.end(); ++it)
	{
		int nClientID = it->first;
		CClientInfo* pClient = it->second;
		assert(pClient != NULL);
		if(pClient->state.nBufSize == 0) continue;

		char chStateFileName[256] = {0};
		char chStateFilePath[256] = {0};
		sprintf(chStateFileName, "%s.%d", sSaveTime.c_str(), nClientID);
		sprintf(chStateFilePath, "./state/%s", chStateFileName);
		NormalizePath(chStateFilePath, false);
		FILE *fp = fopen(chStateFilePath, "wb");
		if(fp != NULL)
		{
			int nWriteSize = (int)fwrite(pClient->state.cBuffer, 1, pClient->state.nBufSize, fp);
			fclose(fp);
			pClient->state.nBufSize = 0; //д�����0��ֹ�´��ظ�д��ͬ������
			oStateFiles.push_back(chStateFileName);
		}
		else
		{
		}
	}
	UNLOCK(m_locker);
#endif
}

void CClientManager::VerWatchRutine(void* param)
{
	CClientManager* me = (CClientManager*)param;
	if(time(0) - me->m_nLastWatchTime < me->m_pConfigure->nVerWatchInterval) return ;
	me->m_nLastWatchTime = (int)time(0);

	LOCK(me->m_pConfigure->locker4ClientApp);
	map<string, CAppUpdateInfo*>::iterator it = me->m_pConfigure->oClientAppVersion.begin();
	for(; it != me->m_pConfigure->oClientAppVersion.end(); ++it)
	{
		CAppUpdateInfo* p = it->second;
		assert(p != NULL);

		//ɨ���AppType�Ƿ���������
		int nAppUpdateVersion = 0;
		string sAppUpdateZipFile = "";
		if(me->ScanClientAppUpdateDir(p->sAppType, p->nAppUpdateVer, nAppUpdateVersion, sAppUpdateZipFile))
		{
			p->nAppUpdateVer = nAppUpdateVersion;
			p->sAppUpdateFile = sAppUpdateZipFile;
		}
	}
	UNLOCK(me->m_pConfigure->locker4ClientApp);
}

bool CClientManager::ScanClientAppUpdateDir(const string& sAppType, 
											int nAppCurVersion, 
											int &nAppUpdateVersion,
											string &sAppUpdateZipFile)
{
	bool bUpdateFound = false;
	nAppUpdateVersion = 0;
	sAppUpdateZipFile = "";

	//���汾����·���Ƿ���ڣ��������򴴽�
	string sAppUpdatePath = m_pConfigure->sClientUpdateDir + sAppType;
	NormalizePath(sAppUpdatePath);
	if(!DIR_EXIST(sAppUpdatePath.c_str()))
	{
		if(!CreateFilePath(sAppUpdatePath.c_str()))
			m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't create file path: %s\n", sAppUpdatePath.c_str());
		return false;
	}

	//ɨ������·�����Ƿ�����Ŀ¼
	CDirScanner ds(sAppUpdatePath.c_str(), false);
	const vector<string>& oVerDirs = ds.GetDirList();
	for(size_t i = 0; i < oVerDirs.size(); i++)
	{
		//��Ŀ¼��������'/'��
		const string& sVerDir = oVerDirs[i];
		string sVerPath = sAppUpdatePath + sVerDir;

		//��ȡ���汾��
		bool bVerFound = true;
		string sVerNo;
		for(size_t j = 0; j < sVerDir.size(); j++)
		{
			char ch = sVerDir[j];
			if(ch <= '9' && ch >= '0')
			{
				sVerNo += ch;
			}
			else if(ch != '/' && ch != '\\')
			{
				m_pLogger->Write(CRunLogger::LOG_WARNING, "Invalid update-folder name, delete it: %s\n", sVerDir.c_str());
				deleteDir(sVerPath.c_str());
				bVerFound = false;
				break;
			}
		}
		if(!bVerFound) continue;
		int nAppVer = atoi(sVerNo.c_str());

		//�汾��С�ڵ�ǰ�汾����ɾ��֮
		if(nAppVer < nAppCurVersion) 
		{
			m_pLogger->Write(CRunLogger::LOG_INFO, "update-folder found but its version is smaller than current, delete it: %s\n", sVerPath.c_str());
			deleteDir(sVerPath.c_str());
			continue;
		}
		if(nAppVer == nAppCurVersion) continue;

		//����Ƿ���"update.ok"�ļ�
		char sAppFlagFile[1024] = {0};
		_snprintf(sAppFlagFile, sizeof(sAppFlagFile), "%s%supdate.ok", sAppUpdatePath.c_str(), sVerDir.c_str());
		if(!DIR_EXIST(sAppFlagFile))
		{
			m_pLogger->Write(CRunLogger::LOG_WARNING, "update.ok not found: %s\n", sAppFlagFile);
			continue;
		}

		//����������ѹ���ļ�
		time_t nTimeNow = time(0);
		char sUpdateZipFile[1024] = {0};
		_snprintf(sUpdateZipFile, sizeof(sUpdateZipFile), "%s%s_%d.myzip", sAppUpdatePath.c_str(), sAppType.c_str(), nAppVer);
		char sZipCmd[1024] = {0};
#ifdef WIN32
		sprintf(sZipCmd, "%s zip %s %s", MARC_MYZIP_APPCMD, sVerPath.c_str(), sUpdateZipFile);
#else
		sprintf(sZipCmd, "/bin/tar -C %s ./ -czf %s", sVerPath.c_str(), sUpdateZipFile);
#endif //WIN32
		m_pLogger->Write(CRunLogger::LOG_INFO, "update-folder found, myzip it: %s\n", sZipCmd);
		if(!Exec(sZipCmd))
		{
			m_pLogger->Write(CRunLogger::LOG_ERROR, "myzip failed: %s\n", sZipCmd);
			continue;
		}
		m_pLogger->Write(CRunLogger::LOG_INFO, "update-zipfile created: %s\n", sUpdateZipFile);

		//���ж����������ֻ�����汾������������
		if(nAppVer > nAppUpdateVersion)
		{
			nAppUpdateVersion = nAppVer;
			sAppUpdateZipFile = sUpdateZipFile;
			bUpdateFound = true;
		}
	}

	return bUpdateFound;
}

void CClientManager::Dump2Html(string& html)
{
	html = "<html><head></head>";
	html += "<script type=\"text/javascript\" src=\"marc.js\"></script>";
	html += "<link type=\"text/css\" rel=\"stylesheet\" href=\"marc.css\" />";
	html += "<body>";

	char buf[2048] = {0};
	_snprintf(buf, sizeof(buf), "<div align=\"center\"><font size=5><p>��ǰ�ܹ���%d��Client�ڵ�</p></font></div>\n", m_oClients.size());
	html += buf;
	html += "<div align=\"center\"><input id=\"cb1\" type=\"checkbox\" name=\"C1\" value=\"ON\" onclick=\"javascript:checkbox_onclick('cb1','detail');\">��������</div>\n";
	html += "<div align=\"center\">";
	html += "<table style=\"font-size: 10pt\" border=\"1\" width=\"1000\" bordercolorlight=\"#C0C0C0\" bordercolordark=\"#FFFFFF\" cellpadding=\"4\">\n";
	html += "<tr>\n";
	html += "	<td bgcolor=\"#F5F4EB\"><span lang=\"zh-cn\">�ڵ���</span></td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">������ַ������</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">�����������</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">����ִ�����</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">����ϴ����</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">�ڵ�״̬</td>\n";
	html += "</tr>";

	LOCK(m_locker);
	CClientInfo* pNodeInfo = NULL;
	
	map<int, CClientInfo*>::iterator it = m_oClients.begin();
	for(; it != m_oClients.end(); ++it)
	{
		pNodeInfo = it->second;
		char status[1024] = {0};
		if(pNodeInfo->bDisabled)
			_snprintf(status, sizeof(status), "<img border=0 src=\"./img/error.png\">(��Ч)");
		else if(pNodeInfo->dNodeStatus.nCurTaskID == 0)
			_snprintf(status, sizeof(status), "<img border=0 src=\"./img/free.png\">(����)");
		else
			_snprintf(status, sizeof(status), "<img border=0 src=\"./img/run.png\">ִ��[%d]", pNodeInfo->dNodeStatus.nCurTaskID);

		string sDisplay = "";
		if(pNodeInfo->dErrLogs.empty())	sDisplay = "display: none; ";

		string sCPUcolor = "";
		if(pNodeInfo->dSourceStatus.cpu_idle_ratio < 10) sCPUcolor = "red";
		string sDiskcolor = "";
		if(pNodeInfo->dSourceStatus.disk_avail_ratio < 10) sDiskcolor = "red";
		string sMemorycolor = "";
		if(pNodeInfo->dSourceStatus.memory_avail_ratio < 10) sMemorycolor = "red";

		_snprintf(buf, sizeof(buf), \
			"<tr bgcolor=\"%s\">\n"
			"	<td><img border=0 src=\"./img/star.png\">%d</td>\n"
			"	<td>%s|%s</td>\n"
			"	<td><img border=0 src=\"./img/down.png\">%d��, %d��/��</td>\n"
			"	<td><img border=0 src=\"./img/ok.png\">%d��, %d��/�� <img border=0 src=\"./img/fail.png\">%d��</td>\n"
			"	<td><img border=0 src=\"./img/up.png\">%d��, %d��/�� <img border=0 src=\"./img/fail.png\">%d�� <img border=0 src=\"./img/up2.png\">%d��</td>\n"
			"	<td>%s</td>\n"
			"</tr>\n",
			pNodeInfo->bDisabled?"red":"#FFCCFF",
			pNodeInfo->nClientID,
			pNodeInfo->sIP.c_str(),
			pNodeInfo->sAppType.c_str(),
			pNodeInfo->dNodeStatus.nTotalFetchedTasks,
			pNodeInfo->dNodeStatus.nTotalFetchedTasks==0?0:pNodeInfo->dNodeStatus.nTotalFecthedTimeUsed/pNodeInfo->dNodeStatus.nTotalFetchedTasks,
			pNodeInfo->dNodeStatus.nTotalFinishedTasks,
			pNodeInfo->dNodeStatus.nTotalFinishedTasks==0?0:pNodeInfo->dNodeStatus.nTotalExecTimeUsed/pNodeInfo->dNodeStatus.nTotalFinishedTasks,
			pNodeInfo->dNodeStatus.nTotalFailedTasks,
			pNodeInfo->dNodeStatus.nTotalFinishedResults,
			pNodeInfo->dNodeStatus.nTotalFinishedResults==0?0:pNodeInfo->dNodeStatus.nTotalUploadedTimeUsed/pNodeInfo->dNodeStatus.nTotalFinishedResults,
			pNodeInfo->dNodeStatus.nTotalFailedResults,
			pNodeInfo->dNodeStatus.nTotalWaitingResults,
			status);
		html += buf;

		if(!pNodeInfo->bDisabled)
		{
			if(pNodeInfo->dNodeStatus.nCurTaskID == 0)
			{
				_snprintf(status, sizeof(status), "%s", "����");
			}
			else
			{
				_snprintf(status, sizeof(status), "��ִ������(id=%d, file=%s, �������ʱ��%s)",
					pNodeInfo->dNodeStatus.nCurTaskID,
					pNodeInfo->dNodeStatus.sLastFetchedFile,
					formatDateTime(pNodeInfo->dNodeStatus.nLastFetchedTime).c_str());
			}
			_snprintf(buf, sizeof(buf), \
				"<tr class=\"detail\">\n"
				"	<td colspan=\"6\">\n"
				"	<ul>\n"
				"		<li style=\"padding-top: 4px; padding-bottom: 4px\">�ڵ㲿��λ��: %s</li>\n"
				"		<li style=\"padding-top: 4px; padding-bottom: 4px\">�ڵ�����ʱ��: %s, ���ע��: %s, �����Ծ: %s, ��������������: %s</li>\n"
				"		<li style=\"padding-top: 4px; padding-bottom: 4px\">�ڵ㵱ǰ״̬: %s</li>\n"
				"		<li style=\"padding-top: 4px; padding-bottom: 4px\">�ڵ���Դ״��: <font color=\"%s\">CPU ������ %d%%</font>, <font color=\"%s\">����ʣ���� %d%%</font>, <font color=\"%s\">�ڴ������ %d%%</font>, ��������: %d bps (��ȡʱ��: %s)</li>\n"
				"		<li style=\"%spadding-top: 4px; padding-bottom: 4px\"><font color=\"red\">����鿴����־: <a target=\"_blank\" href=\"/client_errlog?id=%d\">����쳣��־</a></font></li>\n"
				"	</ul>\n"
				"	</td>\n"
				"</tr>\n",
				pNodeInfo->sInstallPath.c_str(),
				formatDateTime(pNodeInfo->dNodeStatus.nStartupTime).c_str(),
				formatDateTime(pNodeInfo->nRegisterTime).c_str(),
				formatDateTime(pNodeInfo->nLastActiveTime).c_str(),
				formatDateTime(pNodeInfo->dNodeStatus.nLastFetchedTime).c_str(),
				status,
				sCPUcolor.c_str(),
				pNodeInfo->dSourceStatus.cpu_idle_ratio,
				sDiskcolor.c_str(),
				pNodeInfo->dSourceStatus.disk_avail_ratio,
				sMemorycolor.c_str(),
				pNodeInfo->dSourceStatus.memory_avail_ratio,
				pNodeInfo->dSourceStatus.nic_bps,
				formatDateTime(pNodeInfo->dSourceStatus.watch_timestamp).c_str(),
				sDisplay.c_str(),
				pNodeInfo->nClientID); 
			html += buf;
		}
		else
		{
			_snprintf(buf, sizeof(buf), \
				"<tr class=\"detail\">\n"
				"	<td colspan=\"6\">\n"
				"	<ul>\n"
				"		<li style=\"padding-top: 4px; padding-bottom: 4px\">�ڵ㲿��λ��: %s</li>\n"
				"		<li style=\"padding-top: 4px; padding-bottom: 4px\">�ڵ�����ʱ��: %s, ���ע��: %s, �ڵ�ʧЧ: %s, ��������������: %s</li>\n"
				"		<li style=\"padding-top: 4px; padding-bottom: 4px\">�ڵ���Դ״��: <font color=\"%s\">CPU ������ %d%%</font>, <font color=\"%s\">����ʣ���� %d%%</font>, <font color=\"%s\">�ڴ������ %d%%</font>, ��������: %d bps (��ȡʱ��: %s)</li>\n"
				"		<li style=\"%spadding-top: 4px; padding-bottom: 4px\"><font color=\"red\">����鿴����־: <a target=\"_blank\" href=\"/client_errlog?id=%d\">����쳣��־</a></font></li>\n"
				"	</ul>\n"
				"	</td>\n"
				"</tr>\n",
				pNodeInfo->sInstallPath.c_str(),
				formatDateTime(pNodeInfo->dNodeStatus.nStartupTime).c_str(),
				formatDateTime(pNodeInfo->nRegisterTime).c_str(),
				formatDateTime(pNodeInfo->nDisabledTime).c_str(),
				formatDateTime(pNodeInfo->dNodeStatus.nLastFetchedTime).c_str(),
				sCPUcolor.c_str(),
				pNodeInfo->dSourceStatus.cpu_idle_ratio,
				sDiskcolor.c_str(),
				pNodeInfo->dSourceStatus.disk_avail_ratio,
				sMemorycolor.c_str(),
				pNodeInfo->dSourceStatus.memory_avail_ratio,
				pNodeInfo->dSourceStatus.nic_bps,
				formatDateTime(pNodeInfo->dSourceStatus.watch_timestamp).c_str(),
				sDisplay.c_str(),
				pNodeInfo->nClientID);
			html += buf;
		}
	}
	UNLOCK(m_locker);

	html += "</table></div></body></html>";
}

void CClientManager::DumpLog2Html(int nClientID, string &html)
{
}

void CClientManager::DumpErrLog2Html(int nClientID, string &html)
{
	html = "<html><head></head><body style=\"font-size:14px\">";
	LOCK(m_locker);
	map<int, CClientInfo*>::iterator mit = m_oClients.find(nClientID);
	if(mit != m_oClients.end())
	{
		CClientInfo *pNode = mit->second;
		assert(pNode != NULL);
		if(pNode->dErrLogs.empty())
		{
			html += "<font size=6>��ϲ�㣡��Client�ڵ�������û���쳣��־��</font>";
		}
		else
		{
			list<string>::const_iterator it = pNode->dErrLogs.begin();
			for(; it != pNode->dErrLogs.end(); ++it)
			{
				char buf[1024] = {0};
				string color = "#FF0000";
				_snprintf(buf, sizeof(buf), "<li style=\"padding-top: 2px; padding-bottom: 2px\"><font color=\"%s\">%s</font></li>\n", color.c_str(), it->c_str());
				html += buf;
			}
		}
	}
	else
	{
		html += "δ�ҵ���Client�ڵ㣡";
	}
	UNLOCK(m_locker);
	html += "</table></div></body></html>";
}

