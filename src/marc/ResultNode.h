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

//Result�ڵ�
class CResultNode
{
public:
	CResultNode(CResultConf* pConfigure, CRunLogger* pLogger);
	virtual ~CResultNode();

public:
	bool Start(bool bUseBakMaster=false);
	void Stop();

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

	//���App�汾������TRUE���ʾ��Ҫ����
	bool CheckAppVersion(const string& sAppType, _stAppVerInfo& dAppVerInfo);
	
	//App�汾����
	void UpdateAppVersion(const string& sAppType, const _stAppVerInfo& dAppVerInfo);

	//����һ�����ѹ���ļ�
	bool ProcessResultZipFile(const string& sZipFilePath, time_t &nTimeUsed);

	//��ø���AppType�Ľ����������
	bool GetAppCmd(const string& sAppType, string& sAppCmd);

	//����δ����Ľ��ѹ���ļ�
	void SaveUnfinishedResultZipFile();

	//�����ϴ������˳�ʱδ����Ľ��ѹ���ļ�
	void LoadUnfinishedResultZipFile();

	//��Result�ڵ�Ĳ���·����֪Master
	void SendInstallPath();

	//������־��Ϣ
	static bool SendErrLogInfo(const char* sLog, void* arg);

private:
	//�������ڵ�״̬�����߳�
	CLoopThread* m_pHeartThread;
	static void HeartRutine(void *param);
	SOCKET m_nHeartSocket; //ע�ᵽMaster�ڵ������socket�����ڷ�������
	int m_nLastHeartTime; //�ϴ�����ʱ��
	int m_nLastSourceStatusSendTime; //�ϴ���Դʹ��״������ʱ��
	int m_nLastAppVerCheckTime;	//�ϴ�App�汾���ʱ��

	//��������߳�
	CLoopThread* m_pResultThread;
	static void ResultRutine(void *param);
	queue<pair<string,int> > m_oResultZipFiles; //������Ľ��ѹ���ļ����䴦��ʧ�ܴ���
	DEFINE_LOCKER(m_locker);

	_stResultStatus m_stNodeStatus;//�ڵ�����״̬��Ϣ

private:
	SFTP_SVR* m_pSftpSvr; //sftp���������ļ��ϴ����أ�
	static void OnResultUploaded(CResultNode* me, const char* sResultZipFilePath);

	CResultConf* m_pConfigure; //������Ϣ���ⲿ���룩
	CRunLogger* m_pLogger; //��־��¼���ⲿ���룩

	//Master��IP�Ͷ˿�
	string m_sMasterIP;
	unsigned short m_nMasterPort;
	int m_nLastErrCode; //���һ�δ�����
	bool m_bNeedRestart;
};


#endif //_H_RESULTNODE_GDDING_INCUDED_20100203

