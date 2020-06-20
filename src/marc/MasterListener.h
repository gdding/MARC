/*-----------------------------------------------------------------------------
* Copyright (c) 2010~2011 ICT, CAS. All rights reserved.
*   dingguodong@ict.ac.cn OR gdding@hotmail.com
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.
*-----------------------------------------------------------------------------*/
#ifndef _H_MASTERLISTENER_GDDING_INCLUDED_20100127
#define _H_MASTERLISTENER_GDDING_INCLUDED_20100127
#include "../utils/StdHeader.h"
#include "TypeDefs.h"

class CResultNodeManager;
class CTaskManager;
class CLoopThread;
class CRunLogger;
class CMasterConf;
class CClientManager;

class CMasterListener
{
public:
	CMasterListener(CMasterConf* pServerConf, 
					CTaskManager* pTaskManager,
					CResultNodeManager* pResultNodeManager, 
					CClientManager* pClientManager,
					CRunLogger* pLogger);
	virtual ~CMasterListener();

public:
	bool Start(const char *sListenIP,unsigned short nListenPortBase, int nBackLog);
	void Stop();

public:
	//获得监听IP地址
	const char* GetListenIP(){return m_sListenIP.c_str();}

	//获得监听端口
	unsigned short GetListenPort(){return m_nListenPort;}

	//获得当前活跃连接数
	size_t GetActiveConns(){return m_oClientConns.size();}

private:
	//消息处理函数
	void MessageHandler(TClientConn *pClientConn, const _stDataPacket& msg);

	//获得指定Client节点的任务请求信息
	bool GetTaskInfo(int nClientID, const string& sAppType, _stTaskReqInfo &task);

	//获得给定AppType的升级版本
	bool GetClientAppUpdateVersion(const string& sAppType, int nCurAppVersion, _stAppVerInfo &dAppVerInfo);

	//关闭Client节点连接
	void CloseClient(TClientConn* pClientConn);

private:
	//监听线程
	CLoopThread *m_pListenThread;
	static void ListenRutine(void* param);
	list<TClientConn*> m_oClientConns;	//Client节点连接对列

private:
	string m_sListenIP;
	unsigned short m_nListenPort;
	SOCKET m_nListenSocket;
	SOCKET m_nMaxSocket;
	fd_set m_fdAllSet;

private:
	CMasterConf*			m_pConfigure;			//Master节点配置（外部传入）
	CTaskManager*			m_pTaskManager;			//任务管理器（外部传入）
	CClientManager*			m_pClientManager;		//Client节点管理器（外部传入）
	CResultNodeManager*		m_pResultNodeManager;	//Result节点管理器（外部传入）
	CRunLogger*				m_pLogger;				//日志记录（外部传入）

};


#endif //_H_MASTERLISTENER_GDDING_INCLUDED_20100127




