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

//��װ��Result�ڵ���еĲ���
class CResultNodeManager
{
public:
	CResultNodeManager(CMasterConf* pConfigure, CRunLogger* pLogger);
	virtual ~CResultNodeManager();

public:
	//���Result�ڵ�
	bool AddResultNode(int nResultID, const _stResultNode* pResultNode);

	//ɾ��Result�ڵ�
	bool RemoveResultNode(int nResultID);

	//���Result�ڵ���Ŀ
	int NodeCount();

	//����Result�ڵ㣨�ڲ��ҳɹ�ʱ��nResultID���ؽڵ�ID��bDisabled���ظýڵ��Ƿ���Ч��
	bool FindResultNode(const string& ip, unsigned short port, int &nResultID, bool &bDisabled);

	//����AppTypeѡ��������С��Result�ڵ�
	bool SelectResultNode(const string& sAppType, _stResultNode* pResultNode);

	//����Result�ڵ������״̬��Ϣ
	bool SetRunningStatus(int nResultID, const _stResultStatus* pNodeStatus);

	//����Result�ڵ����Դʹ��״����Ϣ
	bool SetSourceStatus(int nResultID, const _stNodeSourceStatus* pSourceStatus);

	//����쳣��־
	bool AddErrLog(int nResultID, const char* sErrLog);

	//���������Ծʱ��
	bool SetActiveTime(int nResultID, time_t t);

	//���ò���·��
	bool SetInstallPath(int nResultID, const char* sInstallPath);

	//ɨ������Ŀ¼���������������������ѹ��������TRUE
	bool ScanResultAppUpdateDir(const string& sAppType, int nAppCurVersion, int &nAppUpdateVersion, string &sAppUpdateZipFile);

public:
	//������Result�ڵ���Ϣ��ʽ�����
	void Dump2Html(string& html);
	void DumpLog2Html(int nResultID, string &html);
	void DumpErrLog2Html(int nResultID, string &html);

private:
	map<int, CResultNodeInfo*> m_oResultNodes; //keyΪResultID
	vector<CResultNodeInfo*> m_dResultNodes;
	int m_nLastFoundNodeIndex;
	unsigned int m_nMaxOverLoad;
	DEFINE_LOCKER(m_locker); //������

private:
	//App�汾����Ŀ¼����߳�
	CLoopThread* m_pVerWatchThread;
	static void VerWatchRutine(void *param);
	int m_nLastWatchTime;

private:
	CMasterConf* m_pConfigure; //Master�ڵ����ã��ⲿ���룩
	CRunLogger* m_pLogger; //��־��¼���ⲿ���룩
};

#endif //_H_RESULTNODEMANAGER_GDDING_INCLUDED_20100126
