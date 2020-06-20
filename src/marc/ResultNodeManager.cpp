#include "ResultNodeManager.h"
#include "../utils/RunLogger.h"
#include "../utils/Utility.h"
#include "../utils/LoopThread.h"
#include "../utils/DirScanner.h"
#include <assert.h>


CResultNodeManager::CResultNodeManager(CMasterConf* pConfigure, CRunLogger* pLogger)
{
	m_nMaxOverLoad = 0;
	m_nLastFoundNodeIndex = -1;
	INITIALIZE_LOCKER(m_locker);
	m_pConfigure = pConfigure;
	m_pLogger = pLogger;

	m_nLastWatchTime = 0;
	m_pVerWatchThread = new CLoopThread();
	assert(m_pVerWatchThread != NULL);
	m_pVerWatchThread->SetRutine(VerWatchRutine, this);
	m_pVerWatchThread->Start();
}

CResultNodeManager::~CResultNodeManager()
{
	m_pVerWatchThread->Stop();
	delete m_pVerWatchThread;

	LOCK(m_locker);
	map<int,CResultNodeInfo*>::iterator it = m_oResultNodes.begin();
	for(; it != m_oResultNodes.end(); it++)
	{
		delete it->second;
	}
	m_oResultNodes.clear();
	m_dResultNodes.clear();
	UNLOCK(m_locker);
	DESTROY_LOCKER(m_locker);
}

bool CResultNodeManager::AddResultNode(int nResultID, const _stResultNode* pResultNode)
{
	LOCK(m_locker);
	CResultNodeInfo* pNodeInfo = NULL;
	map<int, CResultNodeInfo*>::iterator it = m_oResultNodes.find(nResultID);
	if(it == m_oResultNodes.end())
	{
		//为新的ResultID创建新节点
		pNodeInfo = new CResultNodeInfo();
		assert(pNodeInfo != NULL);
		pNodeInfo->pResultNode = (_stResultNode*)malloc(sizeof(_stResultNode));
		assert(pNodeInfo->pResultNode != NULL);
		m_oResultNodes[nResultID] = pNodeInfo;
		m_dResultNodes.push_back(pNodeInfo);
	}
	else
	{
		pNodeInfo = it->second;
		assert(pNodeInfo->bDisabled);
		assert(pNodeInfo->nResultID == nResultID);
	}
	assert(pNodeInfo != NULL);
	pNodeInfo->nResultID = nResultID;
	pNodeInfo->nRegisterTime = time(0);
	pNodeInfo->nLastActiveTime = time(0);
	memcpy(pNodeInfo->pResultNode, pResultNode, sizeof(_stResultNode));
	memset(&pNodeInfo->dNodeStatus, 0, sizeof(_stResultStatus));
	memset(&pNodeInfo->dSourceStatus, 0, sizeof(_stNodeSourceStatus));
	pNodeInfo->dErrLogs.clear();
	pNodeInfo->bDisabled = false;
	pNodeInfo->nDisabledTime = 0;
	UNLOCK(m_locker);

	LOCK(m_pConfigure->locker4ResultApp);
	string sAppType = pResultNode->chAppType;
	if(sAppType.empty())
	{
		map<string, CAppUpdateInfo*>::iterator it = m_pConfigure->oResultAppVersion.begin();
		for(; it != m_pConfigure->oResultAppVersion.end(); ++it)
		{
			const string& sAppType = it->first;
			CAppUpdateInfo* p = it->second;
			
			//扫描是否有升级包
			int nAppUpdateVersion = 0;
			string sAppUpdateZipFile = "";
			if(ScanResultAppUpdateDir(sAppType, 0, nAppUpdateVersion, sAppUpdateZipFile))
			{
				p->nAppUpdateVer = nAppUpdateVersion;
				p->sAppUpdateFile = sAppUpdateZipFile;
			}
		}
	}
	UNLOCK(m_pConfigure->locker4ResultApp);

	return true;
}

bool CResultNodeManager::RemoveResultNode(int nResultID)
{
	bool bRemoved = false;

	LOCK(m_locker);
	map<int, CResultNodeInfo*>::iterator mit = m_oResultNodes.find(nResultID);
	if(mit != m_oResultNodes.end())
	{
		CResultNodeInfo* pNodeInfo = mit->second;
		if(!pNodeInfo->bDisabled)
		{
			m_pLogger->Write(CRunLogger::LOG_WARNING, "Set ResultNode(ID=%d, AppType=%s, %s:%d) to be disabled! %s:%d\n", 
				nResultID, pNodeInfo->pResultNode->chAppType, pNodeInfo->pResultNode->chIp, pNodeInfo->pResultNode->iPort, __FILE__, __LINE__);

			pNodeInfo->bDisabled = true;
			pNodeInfo->nDisabledTime = time(0);
			pNodeInfo->nHeartSocket = INVALID_SOCKET;
			bRemoved = true;
		}
	}
	UNLOCK(m_locker);

	return bRemoved;
}

int CResultNodeManager::NodeCount()
{
	int nCount = 0;
	LOCK(m_locker);
	nCount = m_oResultNodes.size();
	UNLOCK(m_locker);
	return nCount;
}

bool CResultNodeManager::FindResultNode(const string& ip, unsigned short port, int &nResultID, bool &bDisabled)
{
	bool bFound = false;
	nResultID = 0;
	bDisabled = false;

	LOCK(m_locker);
	map<int, CResultNodeInfo*>::iterator it = m_oResultNodes.begin();
	for(; it != m_oResultNodes.end(); it++)
	{
		assert(it->second->pResultNode != NULL);
		if(ip == it->second->pResultNode->chIp  && port == it->second->pResultNode->iPort)
		{
			nResultID = it->first;
			bDisabled = it->second->bDisabled;
			bFound = true;
			break;
		}
	}
	UNLOCK(m_locker);
	return bFound;
}

bool CResultNodeManager::SetRunningStatus(int nResultID, const _stResultStatus* pNodeStatus)
{
	assert(pNodeStatus != NULL);
	bool bFound = false;

	LOCK(m_locker);
	if(pNodeStatus->nOverload > m_nMaxOverLoad)
		m_nMaxOverLoad = pNodeStatus->nOverload;
	map<int, CResultNodeInfo*>::iterator mit = m_oResultNodes.find(nResultID);
	if(mit != m_oResultNodes.end())
	{
		memcpy(&mit->second->dNodeStatus, pNodeStatus, sizeof(_stResultStatus));
		bFound = true;
	}
	UNLOCK(m_locker);
	return bFound;
}

bool CResultNodeManager::SetSourceStatus(int nResultID, const _stNodeSourceStatus* pSourceStatus)
{
	assert(pSourceStatus != NULL);
	bool bFound = false;

	LOCK(m_locker);
	map<int, CResultNodeInfo*>::iterator mit = m_oResultNodes.find(nResultID);
	if(mit != m_oResultNodes.end())
	{
		memcpy(&mit->second->dSourceStatus, pSourceStatus, sizeof(_stNodeSourceStatus));
		bFound = true;
	}
	UNLOCK(m_locker);
	return bFound;
}

bool CResultNodeManager::AddErrLog(int nResultID, const char* sErrLog)
{
	bool bFound = false;

	LOCK(m_locker);
	map<int, CResultNodeInfo*>::iterator mit = m_oResultNodes.find(nResultID);
	if(mit != m_oResultNodes.end())
	{
		mit->second->dErrLogs.push_back(sErrLog);

		//最多保留100条最新日志
		while(mit->second->dErrLogs.size() > MARC_MAX_KEEP_LOGINFO)
		{
			mit->second->dErrLogs.pop_front();
		}
		bFound = true;
	}
	UNLOCK(m_locker);
	return bFound;
}

bool CResultNodeManager::SetActiveTime(int nResultID, time_t t)
{
	bool bFound = false;

	LOCK(m_locker);
	map<int, CResultNodeInfo*>::iterator mit = m_oResultNodes.find(nResultID);
	if(mit != m_oResultNodes.end())
	{
		mit->second->nLastActiveTime = t;
		bFound = true;
	}
	UNLOCK(m_locker);
	return bFound;
}

bool CResultNodeManager::SetInstallPath(int nResultID, const char* sInstallPath)
{
	bool bFound = false;

	LOCK(m_locker);
	map<int, CResultNodeInfo*>::iterator mit = m_oResultNodes.find(nResultID);
	if(mit != m_oResultNodes.end())
	{
		mit->second->sInstallPath = sInstallPath;
		bFound = true;
	}
	UNLOCK(m_locker);
	return bFound;
}

bool CResultNodeManager::SelectResultNode(const string& sAppType, _stResultNode* pResultNode)
{
	//选择负载最小的一个Result节点

	if(pResultNode == NULL) return false;
	bool bFound = false;
	
	LOCK(m_locker);
	unsigned int nMinOverLoad = m_nMaxOverLoad + 1;
	CResultNodeInfo* pNodeFound = NULL;

	//采用轮循方式进行查找，每个节点均有同等机会被轮询到
	int nCount = m_dResultNodes.size();
	int i = m_nLastFoundNodeIndex + 1;
	for(; nCount > 0; --nCount, ++i)
	{
		if(i >= m_dResultNodes.size()) i = 0;
		CResultNodeInfo* pCurNode = m_dResultNodes[i];
		if(pCurNode->bDisabled) continue;

		if (pCurNode->pResultNode->chAppType[0] == 0 ||
			sAppType == pCurNode->pResultNode->chAppType)
		{
			if(pCurNode->dNodeStatus.nOverload == 0)
			{
				pNodeFound = pCurNode;
				m_nLastFoundNodeIndex = i;
				break;
			}
			if(pCurNode->dNodeStatus.nOverload < nMinOverLoad)
			{
				nMinOverLoad = pCurNode->dNodeStatus.nOverload;
				pNodeFound = pCurNode;
				m_nLastFoundNodeIndex = i;
			}
		}
	}

	/* 
	//从头遍历的方式会优先选取第一个负载为0的节点导致其他负载为0的节点得不到轮询机会
	map<int, CResultNodeInfo*>::iterator it = m_oResultNodes.begin();
	for(; it != m_oResultNodes.end(); ++it)
	{
		if(it->second->bDisabled) continue;
		if(it->second->pResultNode->chAppType[0] == 0 || sAppType == it->second->pResultNode->chAppType)
		{
			if(it->second->dNodeStatus.nOverload == 0)
			{
				pNodeFound = it->second;
				break;
			}
			if(it->second->dNodeStatus.nOverload < nMinOverLoad)
			{
				nMinOverLoad = it->second->dNodeStatus.nOverload;
				pNodeFound = it->second;
			}
		}
	} 
	*/

	if(pNodeFound != NULL)
	{
		memcpy(pResultNode, pNodeFound->pResultNode, sizeof(_stResultNode));
		bFound = true;
	}
	UNLOCK(m_locker);

	return bFound;
}

void CResultNodeManager::VerWatchRutine(void* param)
{
	CResultNodeManager* me = (CResultNodeManager*)param;
	if(time(0) - me->m_nLastWatchTime < me->m_pConfigure->nVerWatchInterval) return ;
	me->m_nLastWatchTime = (int)time(0);

	LOCK(me->m_pConfigure->locker4ResultApp);
	map<string, CAppUpdateInfo*>::iterator it = me->m_pConfigure->oResultAppVersion.begin();
	for(; it != me->m_pConfigure->oResultAppVersion.end(); ++it)
	{
		CAppUpdateInfo* p = it->second;
		assert(p != NULL);

		//扫描该AppType是否有升级包
		int nAppUpdateVersion = 0;
		string sAppUpdateZipFile = "";
		if(me->ScanResultAppUpdateDir(p->sAppType, p->nAppUpdateVer, nAppUpdateVersion, sAppUpdateZipFile))
		{
			p->nAppUpdateVer = nAppUpdateVersion;
			p->sAppUpdateFile = sAppUpdateZipFile;
		}
	}
	UNLOCK(me->m_pConfigure->locker4ResultApp);
}

bool CResultNodeManager::ScanResultAppUpdateDir(const string& sAppType, 
												int nAppCurVersion, 
												int &nAppUpdateVersion,
												string &sAppUpdateZipFile)
{
	bool bUpdateFound = false;
	nAppUpdateVersion = 0;
	sAppUpdateZipFile = "";

	//检查版本升级路径是否存在，不存在则创建
	string sAppUpdatePath = m_pConfigure->sResultUpdateDir + sAppType;
	NormalizePath(sAppUpdatePath);
	if(!DIR_EXIST(sAppUpdatePath.c_str()))
	{
		if(!CreateFilePath(sAppUpdatePath.c_str()))
			m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't create file path: %s\n", sAppUpdatePath.c_str());
		return false;
	}

	//扫描升级路径下是否有子目录
	CDirScanner ds(sAppUpdatePath.c_str(), false);
	const vector<string>& oVerDirs = ds.GetDirList();
	for(size_t i = 0; i < oVerDirs.size(); i++)
	{
		//子目录名（含有'/'）
		const string& sVerDir = oVerDirs[i];
		string sVerPath = sAppUpdatePath + sVerDir;

		//提取出版本号
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

		//版本号小于当前版本号则删除之
		if(nAppVer < nAppCurVersion) 
		{
			m_pLogger->Write(CRunLogger::LOG_INFO, "update-folder found but its version is smaller than current, delete it: %s\n", sVerPath.c_str());
			deleteDir(sVerPath.c_str());
			continue;
		}
		if(nAppVer == nAppCurVersion) continue;

		//检查是否含有"update.ok"文件
		char sAppFlagFile[1024] = {0};
		_snprintf(sAppFlagFile, sizeof(sAppFlagFile), "%s%supdate.ok", sAppUpdatePath.c_str(), sVerDir.c_str());
		if(!DIR_EXIST(sAppFlagFile))
		{
			m_pLogger->Write(CRunLogger::LOG_WARNING, "update.ok not found: %s\n", sAppFlagFile);
			continue;
		}

		//生成升级包压缩文件
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

		//若有多个升级包则只保留版本号最大的升级包
		if(nAppVer > nAppUpdateVersion)
		{
			nAppUpdateVersion = nAppVer;
			sAppUpdateZipFile = sUpdateZipFile;
			bUpdateFound = true;
		}
	}

	return bUpdateFound;
}

void CResultNodeManager::Dump2Html(string& html)
{
	html = "<html><head></head>";
	html += "<script type=\"text/javascript\" src=\"marc.js\"></script>";
	html += "<link type=\"text/css\" rel=\"stylesheet\" href=\"marc.css\" />";

	char buf[2048] = {0};
	_snprintf(buf, sizeof(buf), "<div align=\"center\"><font size=5><p>当前总共有%d个Result节点</p></font></div>\n", m_oResultNodes.size());
	html += buf;
	html += "<div align=\"center\"><input id=\"cb1\" type=\"checkbox\" name=\"C1\" value=\"ON\" onclick=\"javascript:checkbox_onclick('cb1','detail');\">隐藏详情</div>\n";
	html += "<div align=\"center\">";
	html += "<table style=\"font-size: 10pt\" border=\"1\" width=\"1000\" bordercolorlight=\"#C0C0C0\" bordercolordark=\"#FFFFFF\" cellpadding=\"4\">\n";
	html += "<tr>\n";
	html += "	<td bgcolor=\"#F5F4EB\"><span lang=\"zh-cn\">节点编号</span></td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">主机地址及类型</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">已接收结果数</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">已处理结果数</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">平均处理时间</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">待处理结果数</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">失败次数</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">抛弃</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">是否有效</td>\n";
	html += "</tr>";

	LOCK(m_locker);
	CResultNodeInfo* pNodeInfo = NULL;
	
	map<int, CResultNodeInfo*>::iterator it = m_oResultNodes.begin();
	for(; it != m_oResultNodes.end(); ++it)
	{
		pNodeInfo = it->second;
		_snprintf(buf, sizeof(buf), \
			"<tr bgcolor=\"%s\">\n"
			"	<td><img border=0 src=\"./img/star.png\">%d</td>\n"
			"	<td>%s:%d|AppType=%s</td>\n"
			"	<td><img border=0 src=\"./img/ok.png\">%d</td>\n"
			"	<td><img border=0 src=\"./img/ok.png\">%d</td>\n"
			"	<td><img border=0 src=\"./img/star.png\">%d 秒</td>\n"
			"	<td><img border=0 src=\"./img/overload.png\">%d</td>\n"
			"	<td><img border=0 src=\"./img/fail.png\">%d 次</td>\n"
			"	<td><img border=0 src=\"./img/alert2.png\">%d</td>\n"
			"	<td>%s</td>\n"
			"</tr>\n",
			pNodeInfo->bDisabled?"red":"#FFCCFF",
			pNodeInfo->nResultID,
			pNodeInfo->pResultNode->chIp, 
			pNodeInfo->pResultNode->iPort,
			pNodeInfo->pResultNode->chAppType,  
			pNodeInfo->dNodeStatus.nTotalReceived,
			pNodeInfo->dNodeStatus.nTotalFinished,
			pNodeInfo->dNodeStatus.nTotalFinished==0?0:pNodeInfo->dNodeStatus.nTotalTimeUsed/pNodeInfo->dNodeStatus.nTotalFinished,
			pNodeInfo->dNodeStatus.nOverload,
			pNodeInfo->dNodeStatus.nTotalFailed,
			pNodeInfo->dNodeStatus.nTotalAbandoned,
			pNodeInfo->bDisabled?"<img border=0 src=\"./img/error.png\">":"<img border=0 src=\"./img/run.png\">");
		html += buf;

		string sDisplay = "";
		if(pNodeInfo->dErrLogs.empty())
			sDisplay = "display: none; ";

		string sCPUcolor = "";
		if(pNodeInfo->dSourceStatus.cpu_idle_ratio < 10) sCPUcolor = "red";
		string sDiskcolor = "";
		if(pNodeInfo->dSourceStatus.disk_avail_ratio < 10) sDiskcolor = "red";
		string sMemorycolor = "";
		if(pNodeInfo->dSourceStatus.memory_avail_ratio < 10) sMemorycolor = "red";

		if(!pNodeInfo->bDisabled)
		{
			_snprintf(buf, sizeof(buf), \
				"<tr class=\"detail\">\n"
				"	<td colspan=\"9\">\n"
				"	<ul>\n"
				"		<li style=\"padding-top: 4px; padding-bottom: 4px\">节点部署位置: %s</li>\n"
				"		<li style=\"padding-top: 4px; padding-bottom: 4px\">节点启动时间: %s, 最近注册: %s, 最近活跃: %s, 最近结果接收完成: %s</li>\n"
				"		<li style=\"padding-top: 4px; padding-bottom: 4px\"><font color=\"%s\">节点资源状况: CPU 空闲率: %d%</font>, <font color=\"%s\">磁盘剩余率 %d%</font>, <font color=\"%s\">内存空闲率 %d%</font>, 网卡速率: %d bps (获取时间: %s)</li>\n"
				"		<li style=\"%spadding-top: 4px; padding-bottom: 4px\"><font color=\"red\">点击查看日志: <a target=\"_blank\" href=\"/result_errlog?id=%d\">最近异常日志</a></font></li>\n"
				"	</ul>\n"
				"	</td>\n"
				"</tr>\n",
				pNodeInfo->sInstallPath.c_str(),
				formatDateTime(pNodeInfo->dNodeStatus.nStartupTime).c_str(),
				formatDateTime(pNodeInfo->nRegisterTime).c_str(),
				formatDateTime(pNodeInfo->nLastActiveTime).c_str(),
				formatDateTime(pNodeInfo->dNodeStatus.nLastReceivedTime).c_str(),
				sCPUcolor.c_str(),
				pNodeInfo->dSourceStatus.cpu_idle_ratio,
				sDiskcolor.c_str(),
				pNodeInfo->dSourceStatus.disk_avail_ratio,
				sMemorycolor.c_str(),
				pNodeInfo->dSourceStatus.memory_avail_ratio,
				pNodeInfo->dSourceStatus.nic_bps,
				formatDateTime(pNodeInfo->dSourceStatus.watch_timestamp).c_str(),
				sDisplay.c_str(),
				pNodeInfo->nResultID);
			html += buf;
		}
		else
		{
			_snprintf(buf, sizeof(buf), \
				"<tr class=\"detail\">\n"
				"	<td colspan=\"9\">\n"
				"	<ul>\n"
				"		<li style=\"padding-top: 4px; padding-bottom: 4px\">节点部署位置: %s</li>\n"
				"		<li style=\"padding-top: 4px; padding-bottom: 4px\">节点启动时间: %s, 最近注册: %s, 节点失效: %s, 最近结果接收完成: %s</li>\n"
				"		<li style=\"padding-top: 4px; padding-bottom: 4px\">节点资源状况: <font color=\"%s\">CPU 空闲率: %d%%</font>, <font color=\"%s\">磁盘剩余率 %d%%</font>, <font color=\"%s\">内存空闲率 %d%%</font>, 网卡速率: %d bps (获取时间: %s)</li>\n"
				"		<li style=\"%spadding-top: 4px; padding-bottom: 4px\"><font color=\"red\">点击查看日志: <a target=\"_blank\" href=\"/result_errlog?id=%d\">最近异常日志</a></font></li>\n"
				"	</ul>\n"
				"	</td>\n"
				"</tr>\n",
				pNodeInfo->sInstallPath.c_str(),
				formatDateTime(pNodeInfo->dNodeStatus.nStartupTime).c_str(),
				formatDateTime(pNodeInfo->nRegisterTime).c_str(),
				formatDateTime(pNodeInfo->nDisabledTime).c_str(),
				formatDateTime(pNodeInfo->dNodeStatus.nLastReceivedTime).c_str(),
				sCPUcolor.c_str(),
				pNodeInfo->dSourceStatus.cpu_idle_ratio,
				sDiskcolor.c_str(),
				pNodeInfo->dSourceStatus.disk_avail_ratio,
				sMemorycolor.c_str(),
				pNodeInfo->dSourceStatus.memory_avail_ratio,
				pNodeInfo->dSourceStatus.nic_bps,
				formatDateTime(pNodeInfo->dSourceStatus.watch_timestamp).c_str(),
				sDisplay.c_str(),
				pNodeInfo->nResultID);
			html += buf;
		}
	}
	UNLOCK(m_locker);

	html += "</table></div></body></html>";
}

void CResultNodeManager::DumpLog2Html(int nResultID, string &html)
{
}

void CResultNodeManager::DumpErrLog2Html(int nResultID, string &html)
{
	html = "<html><head></head><body style=\"font-size:14px\">";
	LOCK(m_locker);
	map<int, CResultNodeInfo*>::iterator mit = m_oResultNodes.find(nResultID);
	if(mit != m_oResultNodes.end())
	{
		CResultNodeInfo *pNode = mit->second;
		assert(pNode != NULL);
		if(pNode->dErrLogs.empty())
		{
			html += "<font size=6>恭喜你！该Result节点启动后没有异常日志。</font>";
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
		html += "未找到该Result节点！";
	}
	UNLOCK(m_locker);
	html += "</table></div></body></html>";
}

