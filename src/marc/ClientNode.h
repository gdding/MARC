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


//Client�ڵ�
class CClientNode
{
public:
	CClientNode(CClientConf* pClientConf, CRunLogger* pLogger);
	virtual ~CClientNode();

public:
	//����, bUseBakMaster��ʾ�Ƿ�ʹ�ñ���Master�ڵ�
	bool Start(bool bUseBakMaster=false);

	//ֹͣ
	void Stop();

	//�Ƿ���Ҫ����
	bool NeedRestart(){return m_bNeedRestart;}

	//�õ����һ�δ�����
	int GetLastErrCode(){return m_nLastErrCode;}

private:
	//��Master�ڵ�ע�ᣬ�ɹ�����0��ʧ�ܷ��ش�����
	int RegisterToMaster();

	//ȡ��ע��
	void UnregisterToMaster();

	//����Master��������Ϣ
	void HandleMasterMsg(const _stDataPacket &msgPacket);

	//����δ�ϴ��Ľ���ļ�
	void SaveUploadResultFiles();

	//�����ϴ��ϴ�δ�ϴ��Ľ���ļ�
	void LoadUploadResultFile();

	//�ڵ�Ӧ�ó�����Ҫ����Ĵ���
	void OnAppNeedTask();

	//����ѹ���ļ�������ɺ�Ĵ���
	void OnTaskDownloaded(int nTaskID, const string& sTaskZipFilePath);

	//��������ʧ�ܵĴ���
	void OnTaskDownloadFailed(int nTaskID, const string& sTaskZipFilePath);

	//�ڵ�Ӧ�ó������н����Ĵ���
	void OnAppFinished();

	//�ڵ�Ӧ�ó�������ʧ�ܵĴ���
	void OnAppFailed();

	//�ڵ�Ӧ�ó������г�ʱ�Ĵ���
	void OnAppTimeout(int nAppRunTime);

	//�ڵ�Ӧ�ó����������еĴ���
	void OnAppRunning();

	//ɱ��Ӧ�ó���
	void KillApp();

	//���Ӧ�ó���״̬��Ϣ
	bool GetAppStateInfo(_stClientState &state);

private:
	//���Master�ڵ�ĴӼ�������˿ڣ��ɹ�����0��ʧ�ܷ��ش�����
	int GetListenerPort(unsigned short& nListenerPort);

	//���App�汾������TRUE���ʾ��Ҫ����
	bool CheckAppVersion(_stAppVerInfo& dAppVerInfo);
	
	//App�汾����
	void UpdateAppVersion(const _stAppVerInfo& dAppVerInfo);

	//���������Ϣ���ɹ�����0��ʧ�ܷ��ش�����
	int GetTaskInfo(_stTaskReqInfo &task);

	//���Result�ڵ��ַ���ɹ�����0��ʧ�ܷ��ش�����
	int GetResultNodeAddr(_stResultNodeAddr &result);

	//�ϴ�����ļ�, �ɹ�����0, ʧ�ܷ��ش�����
	int UploadResultFile(const string& sResultFile);

	//������־��Ϣ
	static bool SendErrLogInfo(const char* sLog, void* arg);

private:
	bool m_bNeedRestart; //�Ƿ���Ҫ����
	int m_nLastErrCode; //���һ�δ�����
	string m_sMasterIP; //Master�ڵ�IP
	unsigned short m_nMasterPort; //Master�ڵ������������˿�
	unsigned short m_nListenerPort; //Master�ڵ�ĴӼ�������˿�
	SOCKET m_nHeartSocket; //��������,Ҳ���ڶ��ڷ���״̬��Ϣ
	fd_set m_fdAllSet;

	//�������ڵ�״̬�����߳�
	CLoopThread* m_pHeartThread;
	static void HeartRutine(void *param);
	int m_nLastHeartTime; //�ϴ�����ʱ��
	int m_nLastAppStateSendTime; //�ϴ�Ӧ�ó���״̬����ʱ��
	int m_nLastSourceStatusSendTime; //�ϴ���Դʹ��״������ʱ��

	//�������߳�
	CLoopThread* m_pTaskThread;
	static void TaskRutine(void *param);
	int m_nAppStartTime; //Ӧ�ó�������ʱ��
	int m_nCurTaskID; //��ǰִ�е�����ID
	int m_nLastTaskFinishedTime; //�ϴ��������ʱ��
	int m_nLastTaskReqFailTime; //�ϴ���������ʧ��ʱ��
	int m_nLastAppVerCheckTime;	//�ϴ�App�汾���ʱ��

	//�첽�ϴ��߳�
	CLoopThread* m_pAsynUploadThread;
	static void AsynUploadRutine(void *param);
	queue<string> m_oUploadFiles; //�첽��ʽ�´�Ŵ��ϴ��Ľ���ļ���ͬ����ʽ�´���ϴ�ʧ�ܵĽ���ļ�
	DEFINE_LOCKER(m_locker);

	//���һ�������õ�Result�ڵ��ַ
    _stResultNodeAddr *m_pResultNodeAddr;

private:
	_stClientStatus m_stNodeStatus; //�ڵ�����״̬��Ϣ
	CClientConf* m_pConfigure; //Client�ڵ�������Ϣ(�ⲿ����)
	CRunLogger* m_pLogger;   //��־��¼���ⲿ���룩

private:
	//����ִ�г���������
	CAppRunner* m_pAppRunner;
	bool m_bAppStarted; //�����Ƿ�������
};


#endif //_H_CLIENTNODE_GDDING_INCLUDED_2010201

