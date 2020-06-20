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
	//��ü���IP��ַ
	const char* GetListenIP(){return m_sListenIP.c_str();}

	//��ü����˿�
	unsigned short GetListenPort(){return m_nListenPort;}

	//��õ�ǰ��Ծ������
	size_t GetActiveConns(){return m_oClientConns.size();}

private:
	//��Ϣ������
	void MessageHandler(TClientConn *pClientConn, const _stDataPacket& msg);

	//���ָ��Client�ڵ������������Ϣ
	bool GetTaskInfo(int nClientID, const string& sAppType, _stTaskReqInfo &task);

	//��ø���AppType�������汾
	bool GetClientAppUpdateVersion(const string& sAppType, int nCurAppVersion, _stAppVerInfo &dAppVerInfo);

	//�ر�Client�ڵ�����
	void CloseClient(TClientConn* pClientConn);

private:
	//�����߳�
	CLoopThread *m_pListenThread;
	static void ListenRutine(void* param);
	list<TClientConn*> m_oClientConns;	//Client�ڵ����Ӷ���

private:
	string m_sListenIP;
	unsigned short m_nListenPort;
	SOCKET m_nListenSocket;
	SOCKET m_nMaxSocket;
	fd_set m_fdAllSet;

private:
	CMasterConf*			m_pConfigure;			//Master�ڵ����ã��ⲿ���룩
	CTaskManager*			m_pTaskManager;			//������������ⲿ���룩
	CClientManager*			m_pClientManager;		//Client�ڵ���������ⲿ���룩
	CResultNodeManager*		m_pResultNodeManager;	//Result�ڵ���������ⲿ���룩
	CRunLogger*				m_pLogger;				//��־��¼���ⲿ���룩

};


#endif //_H_MASTERLISTENER_GDDING_INCLUDED_20100127




