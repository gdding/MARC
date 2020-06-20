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


//Master�ڵ�
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
	//Master���������߳�
	CLoopThread* m_pMasterThread;
	static void MasterRutine(void* param);

	//��Ϣ������
	void MessageHandler(TClientConn *pClientConn, const _stDataPacket& msg);

	//��ø���AppType�������汾
	bool GetResultAppUpdateVersion(const string& sAppType, int nCurAppVersion, _stAppVerInfo &dAppVerInfo);

	//��ø������ٵ�Listener
	CMasterListener* SelectListener();

	//�رտͻ�������
	void CloseClient(TClientConn *pClientConn);

	//��õ�ǰ��Ծ������
	int GetActiveConns();

	SOCKET m_nListenSocket;
	SOCKET m_nMaxSocket;
	fd_set m_fdAllSet;

	//�����ͻ�����Master����socket�����Ӷ���
	list<TClientConn*> m_oClientConns;

private:
	//��������߳�
	CLoopThread* m_pTaskThread;
	static void TaskRutine(void* param);

	//Ϊ����Client�ڵ�ִ���������ɳ���
	bool ExecTaskCreateApp(int nClientID, const string& sAppType);

	//ѹ��������·���µ������ļ���
	bool MyZipTask(const string& sTaskDirName, string& sTaskZipFilePath);

	//��ø���AppType��������������
	bool GetAppCmd(const string& sAppType, string& sAppCmd);

private:
	_stNodeSourceStatus	m_dSourceStatus; //��Դʹ��״����Ϣ
	DEFINE_LOCKER(m_locker);

	//�ڵ���Դ����߳�
	CLoopThread* m_pWatchThread;
	static void WatchRutine(void* param);
	time_t m_nLastWatchTime; //�ϴμ��ʱ��

private:
	//��Master�ڵ���Ϣ��ʽ�������HTML
	void Dump2Html(string& html);

	//��������Result�ڵ����־��Ϣ�����HTML
	void DumpResultLogInfo(int nResultID, string& html);
	void DumpResultErrLogInfo(int nResultID, string& html);

private:
	//״̬�����߳�
	CLoopThread *m_pStateSaveThread;
	static void StateSaveRutine(void* param);
	int m_nLastSaveTime; //�ϴα���ʱ��

private:
	CMasterConf*				m_pConfigure;					//Master�ڵ����ã��ⲿ���룩
	CRunLogger*					m_pLogger;						//��־��¼���ⲿ���룩
	CTaskStatInfo*				m_pTaskStatInfo;				//����ͳ����Ϣ
	CTaskManager*				m_pTaskManager;					//���������
	CResultNodeManager*			m_pResultNodeManager;			//Result�ڵ������
	CClientManager*				m_pClientManager;				//Client�ڵ������
	vector<CMasterListener*>	m_oListeners;					//����������
	map<int,int>				m_oClientID2LastTaskCreateTime; //��¼ÿ��Client�ϴ���������ʧ��ʱ��
	queue<_stDataPacket*>		m_oMsgQueue;					//��������Ϣ����

	static int					m_nClientIDBase;				//Client�ڵ��Ż���
	static int					m_nResultIDBase;				//Result�ڵ��Ż���

private:
	//�������ط���
	string m_sTaskSvrIP;
	unsigned short m_nTaskSvrPort;
	SFTP_SVR* m_pTaskSvr;
	static void OnTaskDownloaded(CMasterNode* me, const char* sTaskZipFilePath);
};


#endif //_H_MASTERNODE_GDDING_INCLUDED_20100128
