/*-----------------------------------------------------------------------------
* Copyright (c) 2010~2011 ICT, CAS. All rights reserved.
*   dingguodong@ict.ac.cn OR gdding@hotmail.com
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.
*-----------------------------------------------------------------------------*/
#ifndef _H_RESULTNODEMANAGER_GDDING_INCLUDED_20100126
#define _H_RESULTNODEMANAGER_GDDING_INCLUDED_20100126
#include "../utils/StdHeader.h"
#include "TypeDefs.h"
class CLoopThread;
class CRunLogger;

//封装对Result节点对列的操作
class CResultNodeManager
{
public:
	CResultNodeManager(CMasterConf* pConfigure, CRunLogger* pLogger);
	virtual ~CResultNodeManager();

public:
	//添加Result节点
	bool AddResultNode(int nResultID, const _stResultNode* pResultNode);

	//删除Result节点
	bool RemoveResultNode(int nResultID);

	//获得Result节点数目
	int NodeCount();

	//查找Result节点（在查找成功时，nResultID返回节点ID，bDisabled返回该节点是否有效）
	bool FindResultNode(const string& ip, unsigned short port, int &nResultID, bool &bDisabled);

	//根据AppType选出负载最小的Result节点
	bool SelectResultNode(const string& sAppType, _stResultNode* pResultNode);

	//设置Result节点的运行状态信息
	bool SetRunningStatus(int nResultID, const _stResultStatus* pNodeStatus);

	//设置Result节点的资源使用状况信息
	bool SetSourceStatus(int nResultID, const _stNodeSourceStatus* pSourceStatus);

	//添加异常日志
	bool AddErrLog(int nResultID, const char* sErrLog);

	//设置最近活跃时间
	bool SetActiveTime(int nResultID, time_t t);

	//设置部署路径
	bool SetInstallPath(int nResultID, const char* sInstallPath);

	//扫描升级目录，若发现有升级包则进行压缩，返回TRUE
	bool ScanResultAppUpdateDir(const string& sAppType, int nAppCurVersion, int &nAppUpdateVersion, string &sAppUpdateZipFile);

public:
	//将各个Result节点信息格式化输出
	void Dump2Html(string& html);
	void DumpLog2Html(int nResultID, string &html);
	void DumpErrLog2Html(int nResultID, string &html);

private:
	map<int, CResultNodeInfo*> m_oResultNodes; //key为ResultID
	vector<CResultNodeInfo*> m_dResultNodes;
	int m_nLastFoundNodeIndex;
	unsigned int m_nMaxOverLoad;
	DEFINE_LOCKER(m_locker); //互斥锁

private:
	//App版本升级目录监控线程
	CLoopThread* m_pVerWatchThread;
	static void VerWatchRutine(void *param);
	int m_nLastWatchTime;

private:
	CMasterConf* m_pConfigure; //Master节点配置（外部传入）
	CRunLogger* m_pLogger; //日志记录（外部传入）
};

#endif //_H_RESULTNODEMANAGER_GDDING_INCLUDED_20100126
