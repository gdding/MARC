/*-----------------------------------------------------------------------------
* Copyright (c) 2010~2011 ICT, CAS. All rights reserved.
*   dingguodong@ict.ac.cn OR gdding@hotmail.com
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.
*-----------------------------------------------------------------------------*/
#ifndef _H_TASKMANAGER_GDDING_INCLUDED_20100126
#define _H_TASKMANAGER_GDDING_INCLUDED_20100126
#include "../utils/StdHeader.h"
#include "TypeDefs.h"

class CLoopThread;
class CMasterConf;
class CTaskStatInfo;
class CRunLogger;

//封装对任务对列的操作
class CTaskManager
{
public:
	CTaskManager(CMasterConf* pConf, CTaskStatInfo* pTaskStat, CRunLogger* pLogger);
	virtual ~CTaskManager();

public:
	//增加任务到对列
	bool AddTask(int nClientID, const string& sAppType, const string& sTaskFilePath, int nTimeUsed);

	//为指定节点请求任务，返回任务文件路径及任务ID
	bool RequestTask(int nClientID, const string& sAppType, string& sTaskFilePath, int& nTaskID);

	//查找指定节点是否有待处理任务
	bool FindTask(int nClientID, const string& sAppType);

	//得到指定节点的待处理任务数
	int TaskCount(int nClientID);

public:
	//设置任务已下载完成
	bool SetTaskFetched(int nClientID, int nTaskID);

	//设置任务已执行完成
	bool SetTaskFinished(int nClientID, int nTaskID);

	//设置任务下载失败
	bool SetTaskFailedByDownload(int nClientID, int nTaskID);

	//设置任务执行失败
	bool SetTaskFailed(int nClientID, int nTaskID);

	//设置指定节点的所有待处理任务均执行失败
	void SetTaskFailed(int nClientID);

public:
	//将任务执行状况信息格式化输出
	void Dump2Html(string& html);

private:
	//任务状态
	typedef enum
	{
		TASK_STATUS_UNKNOWN		= 0, //未知状态
		TASK_STATUS_WAITING		= 1, //任务等待下发
		TASK_STATUS_DOWNLOADING = 2, //任务正被Client下载
		TASK_STATUS_PROCESSING	= 3, //任务正被Client执行
		TASK_STATUS_FINISHED	= 4, //任务已执行完成
		TASK_STATUS_IGNORED		= 5, //任务已失败被抛弃
	}TaskStatus;

	class CTaskInfo
	{
	public:
		int			nTaskID;		//任务ID
		int			nClientID;		//该任务分配的节点ID
		string		sAppType;		//节点应用类型
		string		sTaskFilePath;	//任务文件路径
		int			nTaskFileSize;	//任务文件大小
		time_t		nCreatedTime;	//任务生成时间
		int			nCreatedTimeUsed;//任务生成用时
		time_t		nRequestTime;	//任务请求时间
		time_t		nFetchedTime;	//任务下载完成时间
		time_t		nFinishedTime;	//任务执行完成时间
		time_t		nLastFailedTime;//任务最近失败时间
		int			nFailedCount;	//失败次数
		TaskStatus	nStatus;		//任务状态
	public:
		CTaskInfo();
	};
	typedef list<CTaskInfo*> TaskQueue; //任务对列
	static bool TaskSortByIDdesc(const CTaskInfo* t1, const CTaskInfo* t2);

private:
	static int m_nTaskID; //任务ID

	map<int, TaskQueue*> m_oWaitingTasks; //待处理任务对列（每个节点对应一个任务对列）
	TaskQueue m_oFailedTasks; //处理失败的任务对列
	TaskQueue m_oFinishedTasks;	//已完成的任务队列
	TaskQueue m_oIgnoredTasks; //已抛弃不再处理的任务队列
	DEFINE_LOCKER(m_locker);
	TaskQueue* GetTaskQueue(int nClientID);
	bool Add2TaskQueue(CTaskInfo* pTaskInfo);

	FILE* m_fpIgnoredTasks; //保存因失败而抛弃的任务信息
	void WriteTaskInfo(CTaskInfo* pTaskInfo, FILE* fp);
	void SaveUnfinishedTasks();
	void LoadUnfinishedTasks();

private:
	//任务监控线程
	CLoopThread* m_pTaskWatchThread;
	static void TaskWatchRutine(void* param);
	time_t m_nLastWatchTime;

private:
	CMasterConf* m_pConfigure;
	CRunLogger* m_pLogger;

private:
	CTaskStatInfo* m_pTaskStatInfo;
};

#endif //_H_TASKMANAGER_GDDING_INCLUDED_20100126
