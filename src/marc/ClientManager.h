/*-----------------------------------------------------------------------------
* Copyright (c) 2010~2011 ICT, CAS. All rights reserved.
*   dingguodong@ict.ac.cn OR gdding@hotmail.com
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.
*-----------------------------------------------------------------------------*/
#ifndef _H_CLIENTMANAGER_GDDING_INCLUDED_20110630
#define _H_CLIENTMANAGER_GDDING_INCLUDED_20110630
#include "../utils/StdHeader.h"
#include "TypeDefs.h"
class CLoopThread;
class CRunLogger;
class CTaskManager;

//封装对client节点的管理
class CClientManager
{
public:
	CClientManager(CTaskManager* pTaskManager, CMasterConf* pConfigure, CRunLogger* pLogger);
	virtual ~CClientManager();

public:
	//添加一个Client节点
	bool AddClient(int nClientID, const string& sAppType, const string& sIP, const string& sInstallPath);

	//查找Client节点是否存在
	bool FindClient(int nClientID, bool &bDisabled);
	bool FindClient(const string& sIP, const string& sInstallPath, int &nClientID, bool &bDisabled);

	//删除一个client节点
	bool RemoveClient(int nClientID);

	//获得Client节点数
	int NodeCount();

	//获得需创建任务的client节点
	void GetClientsOfNeedCreateTask(vector<int>& oClientIDs, vector<string>& oClientAppTypes);

public:
	//设置Client节点的活跃时间
	bool SetActiveTime(int nClientID, time_t t);

	//设置是否已请求任务
	bool SetTaskRequested(int nClientID, bool bRequested);

	//设置Client节点的运行状态信息
	bool SetRunningStatus(int nClientID, const _stClientStatus* pNodeStatus);

	//设置Client节点的资源使用状况信息
	bool SetSourceStatus(int nClientID, const _stNodeSourceStatus* pSourceStatus);

	//添加异常日志
	bool AddErrLog(int nClientID, const char* sErrLog);

	//设置Client节点当前处理的任务ID
	bool SetTaskID(int nClientID, int nTaskID);

	//更新Client节点的状态
	bool UpdateClientState(int nClientID, const _stClientState& state);

	//保存Client节点状态
	void SaveClientState(const string& sSaveTime, vector<string>& oStateFiles);

	//扫描给定节点的升级目录，若发现有升级包则进行压缩，返回TRUE
	bool ScanClientAppUpdateDir(const string& sAppType, int nAppCurVersion, int &nAppUpdateVersion, string &sAppUpdateZipFile);

public:
	//将各个Client节点信息格式化输出
	void Dump2Html(string& html);
	void DumpLog2Html(int nClientID, string &html);
	void DumpErrLog2Html(int nClientID, string &html);

private:
	map<int, CClientInfo*> m_oClients; //first为ClientID, second为ClienID对应的Client节点信息
	DEFINE_LOCKER(m_locker); //确保对m_oClients增删改查的互斥操作

private:
	//App版本升级目录监控线程
	CLoopThread* m_pVerWatchThread;
	static void VerWatchRutine(void *param);
	int m_nLastWatchTime;

private:
	CTaskManager* m_pTaskManager;	//待处理任务对列（外部传入）
	CMasterConf* m_pConfigure; //配置（外部传入）
	CRunLogger* m_pLogger; //日志记录（外部传入）
};


#endif //_H_CLIENTMANAGER_GDDING_INCLUDED_20110630
