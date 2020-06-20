/*-----------------------------------------------------------------------------
* Copyright (c) 2010~2011 ICT, CAS. All rights reserved.
*   dingguodong@ict.ac.cn OR gdding@hotmail.com
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.
*-----------------------------------------------------------------------------*/
#ifndef _H_MASTERNODE_GDDING_INCLUDED_20100128
#define _H_MASTERNODE_GDDING_INCLUDED_20100128
#include "../utils/StdHeader.h"
#include "../sftp/sftp_server.h"
#include "TypeDefs.h"

class CLoopThread;
class CTaskManager;
class CClientManager;
class CResultNodeManager;
class CMasterListener;
class CAppRunner;
class CMasterConf;
class CRunLogger;


//Master节点
class CMasterNode
{
public:
	CMasterNode(CMasterConf* pConfingure, CRunLogger* pLogger);
	virtual ~CMasterNode();

public:
	bool Start();
	void Stop();

public:
	const string& HttpdIP(){return m_pConfigure->sIP;}
	int HttpdPort(){return m_pConfigure->nHttpdPort;}

public:
	void Dump2Html(TDumpInfoType t, string& html, int nClientID=0);

private:
	//Master监听服务线程
	CLoopThread* m_pMasterThread;
	static void MasterRutine(void* param);

	//消息处理函数
	void MessageHandler(TClientConn *pClientConn, const _stDataPacket& msg);

	//获得给定AppType的升级版本
	bool GetResultAppUpdateVersion(const string& sAppType, int nCurAppVersion, _stAppVerInfo &dAppVerInfo);

	//获得负载最少的Listener
	CMasterListener* SelectListener();

	//关闭客户端连接
	void CloseClient(TClientConn *pClientConn);

	//获得当前活跃连接数
	int GetActiveConns();

	SOCKET m_nListenSocket;
	SOCKET m_nMaxSocket;
	fd_set m_fdAllSet;

	//各个客户端与Master监听socket的连接对列
	list<TClientConn*> m_oClientConns;

private:
	//任务调度线程
	CLoopThread* m_pTaskThread;
	static void TaskRutine(void* param);

	//为给定Client节点执行任务生成程序
	bool ExecTaskCreateApp(int nClientID, const string& sAppType);

	//压缩任务监控路径下的任务文件夹
	bool MyZipTask(const string& sTaskDirName, string& sTaskZipFilePath);

	//获得给定AppType的任务生成命令
	bool GetAppCmd(const string& sAppType, string& sAppCmd);

private:
	_stNodeSourceStatus	m_dSourceStatus; //资源使用状况信息
	DEFINE_LOCKER(m_locker);

	//节点资源监控线程
	CLoopThread* m_pWatchThread;
	static void WatchRutine(void* param);
	time_t m_nLastWatchTime; //上次监控时间

private:
	//将Master节点信息格式化输出到HTML
	void Dump2Html(string& html);

	//将给定的Result节点的日志信息输出到HTML
	void DumpResultLogInfo(int nResultID, string& html);
	void DumpResultErrLogInfo(int nResultID, string& html);

private:
	//状态保存线程
	CLoopThread *m_pStateSaveThread;
	static void StateSaveRutine(void* param);
	int m_nLastSaveTime; //上次保存时间

private:
	CMasterConf*				m_pConfigure;					//Master节点配置（外部传入）
	CRunLogger*					m_pLogger;						//日志记录（外部传入）
	CTaskStatInfo*				m_pTaskStatInfo;				//任务统计信息
	CTaskManager*				m_pTaskManager;					//任务管理器
	CResultNodeManager*			m_pResultNodeManager;			//Result节点管理器
	CClientManager*				m_pClientManager;				//Client节点管理器
	vector<CMasterListener*>	m_oListeners;					//监听服务组
	map<int,int>				m_oClientID2LastTaskCreateTime; //记录每个Client上次任务生成失败时间
	queue<_stDataPacket*>		m_oMsgQueue;					//待处理消息队列

	static int					m_nClientIDBase;				//Client节点编号基数
	static int					m_nResultIDBase;				//Result节点编号基数

private:
	//任务下载服务
	string m_sTaskSvrIP;
	unsigned short m_nTaskSvrPort;
	SFTP_SVR* m_pTaskSvr;
	static void OnTaskDownloaded(CMasterNode* me, const char* sTaskZipFilePath);
};


#endif //_H_MASTERNODE_GDDING_INCLUDED_20100128
