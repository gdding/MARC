/*-----------------------------------------------------------------------------
* Copyright (c) 2010~2011 ICT, CAS. All rights reserved.
*   dingguodong@ict.ac.cn OR gdding@hotmail.com
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.
*-----------------------------------------------------------------------------*/
#ifndef _H_RESULTNODE_GDDING_INCUDED_20100203
#define _H_RESULTNODE_GDDING_INCUDED_20100203
#include "../utils/StdHeader.h"
#include "../sftp/sftp_server.h"
#include "TypeDefs.h"

class CLoopThread;
class CResultConf;
class CRunLogger;

//Result节点
class CResultNode
{
public:
	CResultNode(CResultConf* pConfigure, CRunLogger* pLogger);
	virtual ~CResultNode();

public:
	bool Start(bool bUseBakMaster=false);
	void Stop();

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

	//检查App版本，返回TRUE则表示需要升级
	bool CheckAppVersion(const string& sAppType, _stAppVerInfo& dAppVerInfo);
	
	//App版本升级
	void UpdateAppVersion(const string& sAppType, const _stAppVerInfo& dAppVerInfo);

	//处理一个结果压缩文件
	bool ProcessResultZipFile(const string& sZipFilePath, time_t &nTimeUsed);

	//获得给定AppType的结果处理命令
	bool GetAppCmd(const string& sAppType, string& sAppCmd);

	//保存未处理的结果压缩文件
	void SaveUnfinishedResultZipFile();

	//载入上次正常退出时未处理的结果压缩文件
	void LoadUnfinishedResultZipFile();

	//将Result节点的部署路径告知Master
	void SendInstallPath();

	//发送日志信息
	static bool SendErrLogInfo(const char* sLog, void* arg);

private:
	//心跳及节点状态发送线程
	CLoopThread* m_pHeartThread;
	static void HeartRutine(void *param);
	SOCKET m_nHeartSocket; //注册到Master节点的连接socket，用于发送心跳
	int m_nLastHeartTime; //上次心跳时间
	int m_nLastSourceStatusSendTime; //上次资源使用状况发送时间
	int m_nLastAppVerCheckTime;	//上次App版本检查时间

	//结果处理线程
	CLoopThread* m_pResultThread;
	static void ResultRutine(void *param);
	queue<pair<string,int> > m_oResultZipFiles; //待处理的结果压缩文件及其处理失败次数
	DEFINE_LOCKER(m_locker);

	_stResultStatus m_stNodeStatus;//节点运行状态信息

private:
	SFTP_SVR* m_pSftpSvr; //sftp服务（用于文件上传下载）
	static void OnResultUploaded(CResultNode* me, const char* sResultZipFilePath);

	CResultConf* m_pConfigure; //配置信息（外部传入）
	CRunLogger* m_pLogger; //日志记录（外部传入）

	//Master的IP和端口
	string m_sMasterIP;
	unsigned short m_nMasterPort;
	int m_nLastErrCode; //最近一次错误码
	bool m_bNeedRestart;
};


#endif //_H_RESULTNODE_GDDING_INCUDED_20100203

