#include "TaskManager.h"
#include "../utils/RunLogger.h"
#include "../utils/LoopThread.h"
#include "../utils/Utility.h"


int CTaskManager::m_nTaskID = 0;
CTaskManager::CTaskInfo::CTaskInfo()
{
	this->nClientID = 0;
	this->nTaskID = 0;
	this->nRequestTime = 0;
	this->nCreatedTime = 0;
	this->nCreatedTimeUsed = 0;
	this->nFetchedTime = 0;
	this->nFinishedTime = 0;
	this->sTaskFilePath = "";
	this->nTaskFileSize = 0;
	this->sAppType = "";
	this->nStatus = TASK_STATUS_UNKNOWN;
	this->nFailedCount = 0;
	this->nLastFailedTime = 0;
}

bool CTaskManager::TaskSortByIDdesc(const CTaskInfo* t1, const CTaskInfo* t2)
{
	return t1->nTaskID > t2->nTaskID;
}

CTaskManager::CTaskManager(CMasterConf* pConf, CTaskStatInfo* pTaskStat, CRunLogger* pLogger)
{
	m_pConfigure = pConf;
	m_pTaskStatInfo = pTaskStat;
	m_pLogger = pLogger;
	INITIALIZE_LOCKER(m_locker);

	m_fpIgnoredTasks = fopen("./log/tasks_ignored.info", "wt");
	assert(m_fpIgnoredTasks != NULL);
	fprintf(m_fpIgnoredTasks, "# Ignored tasks because of timeout or failure\n");
	fprintf(m_fpIgnoredTasks, "%-6s %-8s %-58s %-20s %-20s %-20s %s\n", 
		"TaskID", "ClientID", "TaskFilePath", "CreatedTime", "RequestTime", "FetchedTime", "FinishedTime");

	m_pTaskWatchThread = NULL;
	m_nLastWatchTime = 0;
	if(m_pConfigure->nMaxTaskFetchTime > 0 || m_pConfigure->nMaxTaskRunTime > 0)
	{
		m_pTaskWatchThread = new CLoopThread;
		m_pTaskWatchThread->SetRutine(TaskWatchRutine, this);
		m_pTaskWatchThread->Start();
	}

	//装载上次未完成的任务
	m_pLogger->Write(CRunLogger::LOG_INFO, "Load unfinished tasks cached before...\n");
	LoadUnfinishedTasks();
}

CTaskManager::~CTaskManager()
{
	if(m_pTaskWatchThread != NULL)
	{
		m_pTaskWatchThread->Stop();
		delete m_pTaskWatchThread;
	}

	//保存未完成的任务
	m_pLogger->Write(CRunLogger::LOG_INFO, "Save unfinished tasks to file...\n");
	SaveUnfinishedTasks();

	//清空已完成和抛弃队列
	while(!m_oFinishedTasks.empty())
	{
		CTaskInfo *pTaskInfo = m_oFinishedTasks.front();
		m_oFinishedTasks.pop_front();
		assert(pTaskInfo != NULL);
		delete pTaskInfo;
	}
	while(!m_oIgnoredTasks.empty())
	{
		CTaskInfo *pTaskInfo = m_oIgnoredTasks.front();
		m_oIgnoredTasks.pop_front();
		assert(pTaskInfo != NULL);
		delete pTaskInfo;
	}

	if(m_fpIgnoredTasks != NULL)
		fclose(m_fpIgnoredTasks);

	DESTROY_LOCKER(m_locker);
}

void CTaskManager::TaskWatchRutine(void* param)
{
	CTaskManager* me = (CTaskManager*)param;
	time_t tNowTime = time(0);
	if(tNowTime - me->m_nLastWatchTime < MARC_TASKWATCH_TIME_INTERVAL) return ;
	me->m_nLastWatchTime = tNowTime;

	/* -----------------------------------------------
	* 监控每个未完成任务的下发所用时间和执行所用时间
	* 若超时则放入失败队列或删除之 
	* ------------------------------------------------*/
	LOCK(me->m_locker);
	int nFailedCount = 0;
	map<int, TaskQueue*>::iterator mit = me->m_oWaitingTasks.begin();
	for(; mit != me->m_oWaitingTasks.end(); mit++)
	{
		int nClientID = mit->first;
		TaskQueue* pTaskQueue = mit->second;
		if(pTaskQueue == NULL) continue;

		TaskQueue::iterator qit = pTaskQueue->begin();
		for(; qit != pTaskQueue->end(); )
		{
			CTaskInfo *pTaskInfo = (*qit);
			assert(pTaskInfo != NULL);

			
			bool bTaskFetchTimeout = \
				me->m_pConfigure->nMaxTaskFetchTime > 0 && \
				pTaskInfo->nFetchedTime == 0 && \
				pTaskInfo->nRequestTime > 0 && \
				time(0) - pTaskInfo->nRequestTime > me->m_pConfigure->nMaxTaskFetchTime;
			bool bTaskRunTimeout = \
				me->m_pConfigure->nMaxTaskRunTime > 0 && \
				pTaskInfo->nFinishedTime == 0 && \
				pTaskInfo->nFetchedTime > 0 && \
				time(0) - pTaskInfo->nFetchedTime > me->m_pConfigure->nMaxTaskRunTime;
			if(bTaskFetchTimeout)
			{
				me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Task fetch timeout(TaskID=%d, File=%s) by ClientNode(ClientID=%d), judged to be failed\n",
				pTaskInfo->nTaskID, pTaskInfo->sTaskFilePath.c_str(), pTaskInfo->nClientID);
			}
			if(bTaskRunTimeout)
			{
				me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Task excution timeout(TaskID=%d, File=%s) by ClientNode(ClientID=%d), judged to be failed\n",
				pTaskInfo->nTaskID, pTaskInfo->sTaskFilePath.c_str(), pTaskInfo->nClientID);
			}

			if(bTaskFetchTimeout || bTaskRunTimeout)
			{
				if(me->m_pConfigure->nTaskFailStrategy != MARC_FAILEDTASK_STRATEGY_IGNORE)
				{
					pTaskInfo->nStatus = TASK_STATUS_WAITING;
					me->m_oFailedTasks.push_back(pTaskInfo);
				}
				else
				{
					nFailedCount++;
					me->WriteTaskInfo(pTaskInfo, me->m_fpIgnoredTasks);
					pTaskInfo->nLastFailedTime = time(0);
					pTaskInfo->nStatus = TASK_STATUS_IGNORED;
					me->m_oIgnoredTasks.push_back(pTaskInfo);
				}
				qit = pTaskQueue->erase(qit);
			}
			else
			{
				qit++;
			}
		}
	}

	/* -----------------------------------------------
	* 监控每个已完成任务的内存保持时间, 若超时则清除
	* ------------------------------------------------*/
	while(!me->m_oFinishedTasks.empty())
	{		
		CTaskInfo *pTaskInfo = me->m_oFinishedTasks.front();
		assert(pTaskInfo != NULL);
		if (tNowTime - pTaskInfo->nFinishedTime >= MARC_TASKFINISHED_KEEP_TIME ||
			me->m_oFinishedTasks.size() > MARC_TASKFINISHED_KEEP_SIZE)
		{
			me->m_oFinishedTasks.pop_front();
			delete pTaskInfo;
		}
		else
		{
			//后续的nFinishedTime肯定更大
			break;
		}
	}

	/* -----------------------------------------------
	* 监控每个已抛弃任务的内存保持时间, 若超时则清除
	* ------------------------------------------------*/
	while(!me->m_oIgnoredTasks.empty())
	{
		CTaskInfo *pTaskInfo = me->m_oIgnoredTasks.front();
		assert(pTaskInfo != NULL);
		if(tNowTime - pTaskInfo->nLastFailedTime >= MARC_TASKIGNORED_KEEP_TIME)
		{
			me->m_oIgnoredTasks.pop_front();
			delete pTaskInfo;
		}
		else
		{
			//后续的nLastFailedTime肯定更大
			break;
		}
	}

	UNLOCK(me->m_locker);

	LOCK(me->m_pTaskStatInfo->m_locker);
	me->m_pTaskStatInfo->nTotalFailedTasks += nFailedCount;
	UNLOCK(me->m_pTaskStatInfo->m_locker);
}

//增加任务到对列，返回任务ID；失败返回-1
bool CTaskManager::AddTask(int nClientID, const string& sAppType, const string& sTaskFilePath, int nTimeUsed)
{
	LOCK(m_locker);
	CTaskInfo* pTaskInfo = new CTaskInfo;
	assert(pTaskInfo != NULL);
	pTaskInfo->nTaskID = (++m_nTaskID);
	pTaskInfo->nClientID = nClientID;
	pTaskInfo->sAppType = sAppType;
	pTaskInfo->sTaskFilePath = sTaskFilePath;
	pTaskInfo->nTaskFileSize = getFileSize(sTaskFilePath.c_str());
	pTaskInfo->nCreatedTime = time(0);
	pTaskInfo->nCreatedTimeUsed = nTimeUsed;
	pTaskInfo->nRequestTime = 0;
	pTaskInfo->nFetchedTime = 0;
	pTaskInfo->nFinishedTime = 0;
	pTaskInfo->nFailedCount = 0;
	pTaskInfo->nStatus = TASK_STATUS_WAITING;
	Add2TaskQueue(pTaskInfo);
	UNLOCK(m_locker);

	LOCK(m_pTaskStatInfo->m_locker);
	m_pTaskStatInfo->nLastCreatedTaskID = pTaskInfo->nTaskID;
	m_pTaskStatInfo->nLastCreatedTime = pTaskInfo->nCreatedTime;
	m_pTaskStatInfo->sLastCreatedTaskFile = sTaskFilePath;
	m_pTaskStatInfo->nTotalCreatedTasks++;
	m_pTaskStatInfo->nTotalCreatedTimeUsed += nTimeUsed;
	UNLOCK(m_pTaskStatInfo->m_locker);

	return true;
}

//获得指定节点的任务信息，返回任务文件路径及任务ID
bool CTaskManager::RequestTask(int nClientID, const string& sAppType, string& sTaskFilePath, int& nTaskID)
{
	bool bRet = false;
	LOCK(m_locker);
	if(m_pConfigure->nTaskFailStrategy != MARC_FAILEDTASK_STRATEGY_IGNORE)
	{
		//若有处理失败的同类应用的任务则优先取该任务
		TaskQueue::iterator it = m_oFailedTasks.begin();
		for(; it != m_oFailedTasks.end(); it++)
		{
			CTaskInfo* pTaskInfo = (*it);
			assert(pTaskInfo != NULL);
			if(pTaskInfo->sAppType != sAppType) continue;
			if(m_pConfigure->nTaskFailStrategy==MARC_FAILEDTASK_STRATEGY_KEEP && pTaskInfo->nClientID!=nClientID) continue;
			
			pTaskInfo->nClientID = nClientID;
			pTaskInfo->nRequestTime = time(0);
			pTaskInfo->nFetchedTime = 0;
			pTaskInfo->nFinishedTime = 0;
			pTaskInfo->nStatus = TASK_STATUS_DOWNLOADING;
			Add2TaskQueue(pTaskInfo);
			m_oFailedTasks.erase(it);
			sTaskFilePath = pTaskInfo->sTaskFilePath;
			nTaskID = pTaskInfo->nTaskID;
			bRet = true;
			break;
		}
	}

	TaskQueue* pTaskQueue = (bRet ? NULL : GetTaskQueue(nClientID));
	if(pTaskQueue != NULL)
	{
		TaskQueue::iterator it = pTaskQueue->begin();
		for(; it != pTaskQueue->end(); it++)
		{
			CTaskInfo* pTaskInfo = (*it);
			assert(pTaskInfo != NULL);
			if(pTaskInfo->nRequestTime == 0 && pTaskInfo->sAppType == sAppType)
			{
				pTaskInfo->nRequestTime = time(0);
				pTaskInfo->nStatus = TASK_STATUS_DOWNLOADING;
				sTaskFilePath = pTaskInfo->sTaskFilePath;
				nTaskID = pTaskInfo->nTaskID;
				bRet = true;
				break;
			}
		}
	}
	UNLOCK(m_locker);
	return bRet;
}


bool CTaskManager::SetTaskFetched(int nClientID, int nTaskID)
{
	bool bRet = false;
	LOCK(m_locker);
	TaskQueue* pTaskQueue = GetTaskQueue(nClientID);
	if(pTaskQueue != NULL)
	{
		TaskQueue::iterator it = pTaskQueue->begin();
		for(; it != pTaskQueue->end(); it++)
		{
			CTaskInfo* pTaskInfo = (*it);
			assert(pTaskInfo != NULL);
			if(pTaskInfo->nTaskID == nTaskID)
			{
				pTaskInfo->nFetchedTime = time(0); //记录任务下载完成时间
				pTaskInfo->nStatus = TASK_STATUS_PROCESSING;
				bRet = true;
				break;
			}
		}
	}
	UNLOCK(m_locker);
	if(bRet)
	{
		LOCK(m_pTaskStatInfo->m_locker);
		m_pTaskStatInfo->nTotalDeliveredTasks++;
		UNLOCK(m_pTaskStatInfo->m_locker);
	}
	return bRet;
}

bool CTaskManager::SetTaskFinished(int nClientID, int nTaskID)
{
	bool bRet = false;
	LOCK(m_locker);
	TaskQueue* pTaskQueue = GetTaskQueue(nClientID);
	if(pTaskQueue != NULL)
	{
		TaskQueue::iterator it = pTaskQueue->begin();
		for(; it != pTaskQueue->end(); it++)
		{
			CTaskInfo* pTaskInfo = (*it);
			assert(pTaskInfo != NULL);
			if(pTaskInfo->nTaskID == nTaskID)
			{
				pTaskInfo->nFinishedTime = time(0);
				pTaskInfo->nStatus = TASK_STATUS_FINISHED;
				m_oFinishedTasks.push_back(pTaskInfo);
				pTaskQueue->erase(it);

				//删除任务压缩文件
				if(m_pConfigure->nAutoDeleteTaskFile != 0)
				{
					deleteFile(pTaskInfo->sTaskFilePath.c_str());
				}

				bRet = true;
				break;
			}
		}
	}
	UNLOCK(m_locker);

	if(bRet)
	{
		LOCK(m_pTaskStatInfo->m_locker);
		m_pTaskStatInfo->nTotalFinishdTasks++;
		UNLOCK(m_pTaskStatInfo->m_locker);
	}

	return bRet;
}

bool CTaskManager::SetTaskFailedByDownload(int nClientID, int nTaskID)
{
	bool bRet = false;
	LOCK(m_locker);
	TaskQueue* pTaskQueue = GetTaskQueue(nClientID);
	if(pTaskQueue != NULL)
	{
		TaskQueue::iterator it = pTaskQueue->begin();
		for(; it != pTaskQueue->end(); it++)
		{
			CTaskInfo* pTaskInfo = (*it);
			assert(pTaskInfo != NULL);
			if(pTaskInfo->nTaskID == nTaskID)
			{
				pTaskInfo->nFailedCount++;
				pTaskInfo->nRequestTime = 0;
				pTaskInfo->nFetchedTime = 0;
				pTaskInfo->nFinishedTime = 0;
				pTaskInfo->nStatus = TASK_STATUS_WAITING;
				m_oFailedTasks.push_back(pTaskInfo);
				pTaskQueue->erase(it);
				bRet = true;
				break;
			}
		}
	}
	UNLOCK(m_locker);
	
	return bRet;
}

bool CTaskManager::SetTaskFailed(int nClientID, int nTaskID)
{
	bool bRet = false;
	LOCK(m_locker);
	int nFailedCount = 0;
	TaskQueue* pTaskQueue = GetTaskQueue(nClientID);
	if(pTaskQueue != NULL)
	{
		TaskQueue::iterator it = pTaskQueue->begin();
		for(; it != pTaskQueue->end(); it++)
		{
			CTaskInfo* pTaskInfo = (*it);
			assert(pTaskInfo != NULL);
			if(pTaskInfo->nTaskID == nTaskID)
			{
				pTaskInfo->nLastFailedTime = time(0);
				pTaskInfo->nFailedCount++;
				if (m_pConfigure->nTaskFailStrategy != MARC_FAILEDTASK_STRATEGY_IGNORE &&
					(m_pConfigure->nTaskFailMaxRetry == 0 || m_pConfigure->nTaskFailMaxRetry >= pTaskInfo->nFailedCount))
				{
					pTaskInfo->nStatus = TASK_STATUS_WAITING;
					m_oFailedTasks.push_back(pTaskInfo);
				}
				else
				{
					nFailedCount++;
					pTaskInfo->nStatus = TASK_STATUS_IGNORED;
					WriteTaskInfo(pTaskInfo, m_fpIgnoredTasks);
					m_oIgnoredTasks.push_back(pTaskInfo);
				}
				pTaskQueue->erase(it);
				bRet = true;
				break;
			}
		}
	}
	UNLOCK(m_locker);
	
	LOCK(m_pTaskStatInfo->m_locker);
	m_pTaskStatInfo->nTotalFailedTasks += nFailedCount;
	UNLOCK(m_pTaskStatInfo->m_locker);
	
	return bRet;
}


void CTaskManager::SetTaskFailed(int nClientID)
{
	LOCK(m_locker);
	int nFailedCount = 0;
	TaskQueue* pTaskQueue = GetTaskQueue(nClientID);
	if(pTaskQueue != NULL)
	{
		while(!pTaskQueue->empty())
		{
			CTaskInfo *pTaskInfo = pTaskQueue->front();
			assert(pTaskInfo != NULL);
			pTaskQueue->pop_front();
			pTaskInfo->nLastFailedTime = time(0);
			pTaskInfo->nFailedCount++;
			if(m_pConfigure->nTaskFailStrategy != MARC_FAILEDTASK_STRATEGY_IGNORE)
			{
				pTaskInfo->nStatus = TASK_STATUS_WAITING;
				m_oFailedTasks.push_back(pTaskInfo);
			}
			else
			{
				nFailedCount++;
				pTaskInfo->nStatus = TASK_STATUS_IGNORED;
				WriteTaskInfo(pTaskInfo, m_fpIgnoredTasks);
				m_oIgnoredTasks.push_back(pTaskInfo);
			}
		}
	}
	UNLOCK(m_locker);

	LOCK(m_pTaskStatInfo->m_locker);
	m_pTaskStatInfo->nTotalFailedTasks += nFailedCount;
	UNLOCK(m_pTaskStatInfo->m_locker);
}

//查找指定节点是否有待处理任务（含正处理未完成的任务）
bool CTaskManager::FindTask(int nClientID, const string& sAppType)
{
	bool bFound = false;

	LOCK(m_locker);

	//先查找该Client节点的待处理任务队列
	TaskQueue* pTaskQueue = GetTaskQueue(nClientID);
	if(pTaskQueue != NULL)
	{
		TaskQueue::iterator it = pTaskQueue->begin();
		for(; it != pTaskQueue->end(); it++)
		{
			CTaskInfo* pTaskInfo = (*it);
			if(pTaskInfo->sAppType == sAppType)
			{
				bFound = true;
				break;
			}
		}
	}

	//未找到则查找失败任务队列
	if(!bFound && m_pConfigure->nTaskFailStrategy != MARC_FAILEDTASK_STRATEGY_IGNORE)
	{
		TaskQueue::iterator it = m_oFailedTasks.begin();
		for(; it != m_oFailedTasks.end(); it++)
		{
			CTaskInfo* pTaskInfo = (*it);
			switch(m_pConfigure->nTaskFailStrategy)
			{
			case MARC_FAILEDTASK_STRATEGY_KEEP:
				bFound = (pTaskInfo->nClientID==nClientID && pTaskInfo->sAppType==sAppType);
				break;
			case MARC_FAILEDTASK_STRATEGY_AJUST:
				bFound = (pTaskInfo->sAppType==sAppType);
				break;
			default:
				m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid TaskFailStrategy %d\n", m_pConfigure->nTaskFailStrategy);
				break;
			};
			if(bFound) break;
		}
	}

	UNLOCK(m_locker);
	return bFound;
}

//得到指定节点的待处理任务数
int CTaskManager::TaskCount(int nClientID)
{
	int nCount = 0;
	LOCK(m_locker);
	TaskQueue* pTaskQueue = GetTaskQueue(nClientID);
	if(pTaskQueue != NULL)
		nCount = (int)pTaskQueue->size();
	UNLOCK(m_locker);
	return nCount;
}

bool CTaskManager::Add2TaskQueue(CTaskInfo* pTaskInfo)
{
	assert(pTaskInfo != NULL);
	int nClientID = pTaskInfo->nClientID;
	TaskQueue* pTaskQueue = GetTaskQueue(nClientID);
	if(pTaskQueue == NULL)
	{
		pTaskQueue = new TaskQueue;
		assert(pTaskQueue != NULL);
		m_oWaitingTasks[nClientID] = pTaskQueue;
	}
	pTaskQueue->push_back(pTaskInfo);
	return true;
}

CTaskManager::TaskQueue* CTaskManager::GetTaskQueue(int nClientID)
{
	TaskQueue* pTaskQueue = NULL;
	map<int, TaskQueue*>::const_iterator it = m_oWaitingTasks.find(nClientID);
	if(it != m_oWaitingTasks.end())
	{
		pTaskQueue = it->second;
	}
	return pTaskQueue;
}

void CTaskManager::WriteTaskInfo(CTaskInfo* pTaskInfo, FILE* fp)
{
	if(fp != NULL)
	{
		fprintf(fp, 
			"%-6d %-8d %-58s %-20s %-20s %-20s %s\n", 
			pTaskInfo->nTaskID,
			pTaskInfo->nClientID,
			pTaskInfo->sTaskFilePath.c_str(),
			formatDateTime(pTaskInfo->nCreatedTime).c_str(),
			formatDateTime(pTaskInfo->nRequestTime).c_str(),
			formatDateTime(pTaskInfo->nFetchedTime).c_str(),
			formatDateTime(pTaskInfo->nFinishedTime).c_str());
		fflush(fp);
	}
}

void CTaskManager::SaveUnfinishedTasks()
{
	FILE *fp = fopen(MARC_UNFINISHED_TASK_LISTFILE, "wt");
	if(fp == NULL)
	{
		this->m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't create file %s\n", MARC_UNFINISHED_TASK_LISTFILE);
		return ;
	}

	LOCK(m_locker);
	map<int, TaskQueue*>::iterator it;
	for(it = m_oWaitingTasks.begin(); it != m_oWaitingTasks.end(); it++)
	{
		TaskQueue* q = it->second;
		while(!q->empty())
		{
			CTaskInfo *pTaskInfo = q->front();
			q->pop_front();
			assert(pTaskInfo != NULL);
			if(m_pConfigure->nAutoSaveUnfinishedTask != 0)
			{
				fprintf(fp, "%-10d %-20s %s %d\n", 
						pTaskInfo->nClientID, 
						pTaskInfo->sAppType.c_str(),
						pTaskInfo->sTaskFilePath.c_str(),
						pTaskInfo->nCreatedTime);
			}
			else
			{
				WriteTaskInfo(pTaskInfo, m_fpIgnoredTasks);
			}
			delete pTaskInfo;
		}
		delete q;
	}
	m_oWaitingTasks.clear();

	while(!m_oFailedTasks.empty())
	{
		CTaskInfo *pTaskInfo = m_oFailedTasks.front();
		m_oFailedTasks.pop_front();
		assert(pTaskInfo != NULL);
		if(m_pConfigure->nAutoSaveUnfinishedTask != 0)
		{
			fprintf(fp, "%-10d %-20s %s %d\n", 
					pTaskInfo->nClientID, 
					pTaskInfo->sAppType.c_str(), 
					pTaskInfo->sTaskFilePath.c_str(),
					pTaskInfo->nCreatedTime);
		}
		else
		{
			WriteTaskInfo(pTaskInfo, m_fpIgnoredTasks);
		}
		delete pTaskInfo;
	}
	m_oFailedTasks.clear();
	UNLOCK(m_locker);

	fclose(fp);
}

void CTaskManager::LoadUnfinishedTasks()
{
	assert(m_pConfigure != NULL);
	if(m_pConfigure->nAutoSaveUnfinishedTask == 0) return ;

	FILE *fp = fopen(MARC_UNFINISHED_TASK_LISTFILE, "rt");
	if(fp != NULL)
	{
		char sLine[1024] = {0};
		while (fgets(sLine, 1024, fp))
		{
			int nClientID = 0;
			char sAppType[256] = {0};
			char sTaskFile[512] = {0};
			int nCreatedTime = 0;
			sscanf(sLine,"%d %s %s %d", &nClientID, sAppType, sTaskFile, &nCreatedTime);
			if(sAppType[0] == 0 || sTaskFile[0] == 0) continue;
			if(!DIR_EXIST(sTaskFile))
			{
				m_pLogger->Write(CRunLogger::LOG_WARNING, "Task file not existed: %s\n", sTaskFile);
				continue;
			}
			CTaskInfo* pTaskInfo = new CTaskInfo;
			assert(pTaskInfo != NULL);
			pTaskInfo->nTaskID = (++m_nTaskID);
			pTaskInfo->nClientID = nClientID;
			pTaskInfo->sAppType = sAppType;
			pTaskInfo->sTaskFilePath = sTaskFile;
			pTaskInfo->nTaskFileSize = getFileSize(sTaskFile);
			pTaskInfo->nCreatedTime = nCreatedTime;
			pTaskInfo->nRequestTime = 0;
			pTaskInfo->nFetchedTime = 0;
			pTaskInfo->nFinishedTime = 0;
			pTaskInfo->nFailedCount = 0;
			pTaskInfo->nLastFailedTime = 0;
			pTaskInfo->nStatus = TASK_STATUS_WAITING;
			m_oFailedTasks.push_back(pTaskInfo);
		}
		fclose(fp);
	}
}

void CTaskManager::Dump2Html(string& html)
{
	html = "<html><head></head>";
	html += "<script type=\"text/javascript\" src=\"marc.js\"></script>";
	html += "<link type=\"text/css\" rel=\"stylesheet\" href=\"marc.css\" />";
	html += "<body>";
	html += "<div align=\"center\"><font size=5><p>任  务  状  态  列  表</p></font></div>\n";
	html += "<div align=\"center\"><input id=\"cb1\" type=\"checkbox\" name=\"C1\" value=\"ON\" onclick=\"javascript:checkbox_onclick('cb1','detail');\">隐藏详情</div>\n";
	html += "<div align=\"center\">";
	html += "<table style=\"font-size: 10pt\" border=\"1\" width=\"1000\" bordercolorlight=\"#C0C0C0\" bordercolordark=\"#FFFFFF\" cellpadding=\"4\">\n";
	html += "<tr>\n";
	html += "	<td bgcolor=\"#F5F4EB\"><span lang=\"zh-cn\">任务ID及类型</span></td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">任务生成时间</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">任务请求时间</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">下载完成时间</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">执行完成时间</td>\n";
	html += "	<td bgcolor=\"#F5F4EB\">任务状态</td>\n";
	html += "</tr>";

	#define TASK_QUEUE_APPEND(dest,src) \
	for(qit = (src).begin(); qit != (src).end(); qit++) \
	{ \
		CTaskInfo *pTaskInfo = (*qit); \
		assert(pTaskInfo != NULL); \
		(dest).push_back(pTaskInfo); \
	}

	TaskQueue oTaskQueue;
	TaskQueue::iterator qit;

	LOCK(m_locker);

	//获得待处理队列、已完成队列、已抛弃队列的所有任务
	map<int, TaskQueue*>::iterator mit = m_oWaitingTasks.begin();
	for(; mit != m_oWaitingTasks.end(); ++mit)
	{
		int nClientID = mit->first;
		TaskQueue* pTaskQueue = mit->second;
		if(pTaskQueue == NULL) continue;
		TASK_QUEUE_APPEND(oTaskQueue, *pTaskQueue);
	}
	TASK_QUEUE_APPEND(oTaskQueue, m_oFailedTasks);
	TASK_QUEUE_APPEND(oTaskQueue, m_oFinishedTasks);
	TASK_QUEUE_APPEND(oTaskQueue, m_oIgnoredTasks);

	//根据任务ID降序排列
	oTaskQueue.sort(TaskSortByIDdesc);

	//输出到HTML
	for(qit = oTaskQueue.begin(); qit != oTaskQueue.end(); ++qit)
	{
		char buf[2048] = {0};
		const CTaskInfo* pTaskInfo = (*qit);
		string status = "未知";
		string color = "#999966";
		switch(pTaskInfo->nStatus)
		{
		case TASK_STATUS_WAITING:
			status = "等待下发..."; color = "#6699FF";
			break;
		case TASK_STATUS_DOWNLOADING:
			status = "正在下发..."; color = "#FFCCFF";
			break;
		case TASK_STATUS_PROCESSING:
			status = "正在执行..."; color = "#CCFF33";
			break;
		case TASK_STATUS_FINISHED:
			sprintf(buf, "已完成(%d秒)", pTaskInfo->nFinishedTime-pTaskInfo->nRequestTime);
			status = buf; color = "#009933";
			break;
		case TASK_STATUS_IGNORED:
			status = "失败被抛弃"; color = "#FF3300";
			break;
		default:
			break;
		};
		if(pTaskInfo->nStatus != TASK_STATUS_IGNORED && pTaskInfo->nFailedCount > 0) color = "#66FFFF";

		_snprintf(buf, sizeof(buf), \
			"<tr bgcolor=\"%s\">\n"
			"	<td>%d|%s</td>\n"
			"	<td>%s(%d秒)</td>\n"
			"	<td>%s</td>\n"
			"	<td>%s(%d秒)</td>\n"
			"	<td>%s(%d秒)</td>\n"
			"	<td>%s</td>\n"
			"</tr>\n",
			color.c_str(),
			pTaskInfo->nTaskID,
			pTaskInfo->sAppType.c_str(),
			formatDateTime(pTaskInfo->nCreatedTime).c_str(),
			pTaskInfo->nCreatedTimeUsed,
			pTaskInfo->nRequestTime==0 ? "--------" : formatDateTime(pTaskInfo->nRequestTime).c_str(),
			pTaskInfo->nFetchedTime==0 ? "--------" : formatDateTime(pTaskInfo->nFetchedTime).c_str(),
			pTaskInfo->nFetchedTime==0 ? 0 : pTaskInfo->nFetchedTime - pTaskInfo->nRequestTime,
			pTaskInfo->nFinishedTime==0 ? "--------" : formatDateTime(pTaskInfo->nFinishedTime).c_str(),
			pTaskInfo->nFinishedTime==0 ? 0 : pTaskInfo->nFinishedTime - pTaskInfo->nFetchedTime,
			status.c_str());
		html += buf;

		_snprintf(buf, sizeof(buf), \
			"<tr class=\"detail\">\n"
			"	<td colspan=\"6\">\n"
			"	任务分配给: ClientID=%d, 任务大小: %dBytes, 任务文件: %s</li>\n"
			"	</td>\n"
			"</tr>\n",
			pTaskInfo->nClientID,
			pTaskInfo->nTaskFileSize,
			pTaskInfo->sTaskFilePath.c_str());
		html += buf;
	}


	UNLOCK(m_locker);

	html += "</table></div></body></html>";
}

