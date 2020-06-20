/*-----------------------------------------------------------------------------
* Copyright (c) 2010~2011 ICT, CAS. All rights reserved.
*   dingguodong@ict.ac.cn OR gdding@hotmail.com
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.
*-----------------------------------------------------------------------------*/
#ifndef _H_CLIENTNODE_GDDING_INCLUDED_2010201
#define _H_CLIENTNODE_GDDING_INCLUDED_2010201
#include "../utils/StdHeader.h"
#include "TypeDefs.h"

class CLoopThread;
class CClientConf;
class CAppRunner;
class CRunLogger;


//Client节点
class CClientNode
{
public:
	CClientNode(CClientConf* pClientConf, CRunLogger* pLogger);
	virtual ~CClientNode();

public:
	//启动, bUseBakMaster表示是否使用备用Master节点
	bool Start(bool bUseBakMaster=false);

	//停止
	void Stop();

	//是否需要重启
	bool NeedRestart(){return m_bNeedRestart;}

	//得到最近一次错误码
	int GetLastErrCode(){return m_nLastErrCode;}

private:
	//向Master节点注册，成功返回0，失败返回错误码
	int RegisterToMaster();

	//取消注册
	void UnregisterToMaster();

	//处理Master发来的消息
	void HandleMasterMsg(const _stDataPacket &msgPacket);

	//保存未上传的结果文件
	void SaveUploadResultFiles();

	//重新上传上次未上传的结果文件
	void LoadUploadResultFile();

	//节点应用程序需要任务的处理
	void OnAppNeedTask();

	//任务压缩文件下载完成后的处理
	void OnTaskDownloaded(int nTaskID, const string& sTaskZipFilePath);

	//任务下载失败的处理
	void OnTaskDownloadFailed(int nTaskID, const string& sTaskZipFilePath);

	//节点应用程序运行结束的处理
	void OnAppFinished();

	//节点应用程序运行失败的处理
	void OnAppFailed();

	//节点应用程序运行超时的处理
	void OnAppTimeout(int nAppRunTime);

	//节点应用程序正在运行的处理
	void OnAppRunning();

	//杀死应用程序
	void KillApp();

	//获得应用程序状态信息
	bool GetAppStateInfo(_stClientState &state);

private:
	//获得Master节点的从监听服务端口，成功返回0，失败返回错误码
	int GetListenerPort(unsigned short& nListenerPort);

	//检查App版本，返回TRUE则表示需要升级
	bool CheckAppVersion(_stAppVerInfo& dAppVerInfo);
	
	//App版本升级
	void UpdateAppVersion(const _stAppVerInfo& dAppVerInfo);

	//获得任务信息，成功返回0，失败返回错误码
	int GetTaskInfo(_stTaskReqInfo &task);

	//获得Result节点地址，成功返回0，失败返回错误码
	int GetResultNodeAddr(_stResultNodeAddr &result);

	//上传结果文件, 成功返回0, 失败返回错误码
	int UploadResultFile(const string& sResultFile);

	//发送日志信息
	static bool SendErrLogInfo(const char* sLog, void* arg);

private:
	bool m_bNeedRestart; //是否需要重启
	int m_nLastErrCode; //最近一次错误码
	string m_sMasterIP; //Master节点IP
	unsigned short m_nMasterPort; //Master节点的主监听服务端口
	unsigned short m_nListenerPort; //Master节点的从监听服务端口
	SOCKET m_nHeartSocket; //心跳连接,也用于定期发送状态信息
	fd_set m_fdAllSet;

	//心跳及节点状态发送线程
	CLoopThread* m_pHeartThread;
	static void HeartRutine(void *param);
	int m_nLastHeartTime; //上次心跳时间
	int m_nLastAppStateSendTime; //上次应用程序状态发送时间
	int m_nLastSourceStatusSendTime; //上次资源使用状况发送时间

	//任务处理线程
	CLoopThread* m_pTaskThread;
	static void TaskRutine(void *param);
	int m_nAppStartTime; //应用程序启动时间
	int m_nCurTaskID; //当前执行的任务ID
	int m_nLastTaskFinishedTime; //上次任务完成时间
	int m_nLastTaskReqFailTime; //上次任务请求失败时间
	int m_nLastAppVerCheckTime;	//上次App版本检查时间

	//异步上传线程
	CLoopThread* m_pAsynUploadThread;
	static void AsynUploadRutine(void *param);
	queue<string> m_oUploadFiles; //异步方式下存放待上传的结果文件，同步方式下存放上传失败的结果文件
	DEFINE_LOCKER(m_locker);

	//最近一次请求获得的Result节点地址
    _stResultNodeAddr *m_pResultNodeAddr;

private:
	_stClientStatus m_stNodeStatus; //节点运行状态信息
	CClientConf* m_pConfigure; //Client节点配置信息(外部传入)
	CRunLogger* m_pLogger;   //日志记录（外部传入）

private:
	//任务执行程序运行器
	CAppRunner* m_pAppRunner;
	bool m_bAppStarted; //程序是否已启动
};


#endif //_H_CLIENTNODE_GDDING_INCLUDED_2010201

