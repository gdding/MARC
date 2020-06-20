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

//��װ��������еĲ���
class CTaskManager
{
public:
	CTaskManager(CMasterConf* pConf, CTaskStatInfo* pTaskStat, CRunLogger* pLogger);
	virtual ~CTaskManager();

public:
	//�������񵽶���
	bool AddTask(int nClientID, const string& sAppType, const string& sTaskFilePath, int nTimeUsed);

	//Ϊָ���ڵ��������񣬷��������ļ�·��������ID
	bool RequestTask(int nClientID, const string& sAppType, string& sTaskFilePath, int& nTaskID);

	//����ָ���ڵ��Ƿ��д���������
	bool FindTask(int nClientID, const string& sAppType);

	//�õ�ָ���ڵ�Ĵ�����������
	int TaskCount(int nClientID);

public:
	//�����������������
	bool SetTaskFetched(int nClientID, int nTaskID);

	//����������ִ�����
	bool SetTaskFinished(int nClientID, int nTaskID);

	//������������ʧ��
	bool SetTaskFailedByDownload(int nClientID, int nTaskID);

	//��������ִ��ʧ��
	bool SetTaskFailed(int nClientID, int nTaskID);

	//����ָ���ڵ�����д����������ִ��ʧ��
	void SetTaskFailed(int nClientID);

public:
	//������ִ��״����Ϣ��ʽ�����
	void Dump2Html(string& html);

private:
	//����״̬
	typedef enum
	{
		TASK_STATUS_UNKNOWN		= 0, //δ֪״̬
		TASK_STATUS_WAITING		= 1, //����ȴ��·�
		TASK_STATUS_DOWNLOADING = 2, //��������Client����
		TASK_STATUS_PROCESSING	= 3, //��������Clientִ��
		TASK_STATUS_FINISHED	= 4, //������ִ�����
		TASK_STATUS_IGNORED		= 5, //������ʧ�ܱ�����
	}TaskStatus;

	class CTaskInfo
	{
	public:
		int			nTaskID;		//����ID
		int			nClientID;		//���������Ľڵ�ID
		string		sAppType;		//�ڵ�Ӧ������
		string		sTaskFilePath;	//�����ļ�·��
		int			nTaskFileSize;	//�����ļ���С
		time_t		nCreatedTime;	//��������ʱ��
		int			nCreatedTimeUsed;//����������ʱ
		time_t		nRequestTime;	//��������ʱ��
		time_t		nFetchedTime;	//�����������ʱ��
		time_t		nFinishedTime;	//����ִ�����ʱ��
		time_t		nLastFailedTime;//�������ʧ��ʱ��
		int			nFailedCount;	//ʧ�ܴ���
		TaskStatus	nStatus;		//����״̬
	public:
		CTaskInfo();
	};
	typedef list<CTaskInfo*> TaskQueue; //�������
	static bool TaskSortByIDdesc(const CTaskInfo* t1, const CTaskInfo* t2);

private:
	static int m_nTaskID; //����ID

	map<int, TaskQueue*> m_oWaitingTasks; //������������У�ÿ���ڵ��Ӧһ��������У�
	TaskQueue m_oFailedTasks; //����ʧ�ܵ��������
	TaskQueue m_oFinishedTasks;	//����ɵ��������
	TaskQueue m_oIgnoredTasks; //���������ٴ�����������
	DEFINE_LOCKER(m_locker);
	TaskQueue* GetTaskQueue(int nClientID);
	bool Add2TaskQueue(CTaskInfo* pTaskInfo);

	FILE* m_fpIgnoredTasks; //������ʧ�ܶ�������������Ϣ
	void WriteTaskInfo(CTaskInfo* pTaskInfo, FILE* fp);
	void SaveUnfinishedTasks();
	void LoadUnfinishedTasks();

private:
	//�������߳�
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
