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

//��װ��client�ڵ�Ĺ���
class CClientManager
{
public:
	CClientManager(CTaskManager* pTaskManager, CMasterConf* pConfigure, CRunLogger* pLogger);
	virtual ~CClientManager();

public:
	//���һ��Client�ڵ�
	bool AddClient(int nClientID, const string& sAppType, const string& sIP, const string& sInstallPath);

	//����Client�ڵ��Ƿ����
	bool FindClient(int nClientID, bool &bDisabled);
	bool FindClient(const string& sIP, const string& sInstallPath, int &nClientID, bool &bDisabled);

	//ɾ��һ��client�ڵ�
	bool RemoveClient(int nClientID);

	//���Client�ڵ���
	int NodeCount();

	//����贴�������client�ڵ�
	void GetClientsOfNeedCreateTask(vector<int>& oClientIDs, vector<string>& oClientAppTypes);

public:
	//����Client�ڵ�Ļ�Ծʱ��
	bool SetActiveTime(int nClientID, time_t t);

	//�����Ƿ�����������
	bool SetTaskRequested(int nClientID, bool bRequested);

	//����Client�ڵ������״̬��Ϣ
	bool SetRunningStatus(int nClientID, const _stClientStatus* pNodeStatus);

	//����Client�ڵ����Դʹ��״����Ϣ
	bool SetSourceStatus(int nClientID, const _stNodeSourceStatus* pSourceStatus);

	//����쳣��־
	bool AddErrLog(int nClientID, const char* sErrLog);

	//����Client�ڵ㵱ǰ���������ID
	bool SetTaskID(int nClientID, int nTaskID);

	//����Client�ڵ��״̬
	bool UpdateClientState(int nClientID, const _stClientState& state);

	//����Client�ڵ�״̬
	void SaveClientState(const string& sSaveTime, vector<string>& oStateFiles);

	//ɨ������ڵ������Ŀ¼���������������������ѹ��������TRUE
	bool ScanClientAppUpdateDir(const string& sAppType, int nAppCurVersion, int &nAppUpdateVersion, string &sAppUpdateZipFile);

public:
	//������Client�ڵ���Ϣ��ʽ�����
	void Dump2Html(string& html);
	void DumpLog2Html(int nClientID, string &html);
	void DumpErrLog2Html(int nClientID, string &html);

private:
	map<int, CClientInfo*> m_oClients; //firstΪClientID, secondΪClienID��Ӧ��Client�ڵ���Ϣ
	DEFINE_LOCKER(m_locker); //ȷ����m_oClients��ɾ�Ĳ�Ļ������

private:
	//App�汾����Ŀ¼����߳�
	CLoopThread* m_pVerWatchThread;
	static void VerWatchRutine(void *param);
	int m_nLastWatchTime;

private:
	CTaskManager* m_pTaskManager;	//������������У��ⲿ���룩
	CMasterConf* m_pConfigure; //���ã��ⲿ���룩
	CRunLogger* m_pLogger; //��־��¼���ⲿ���룩
};


#endif //_H_CLIENTMANAGER_GDDING_INCLUDED_20110630
