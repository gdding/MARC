#include "ClientNode.h"
#include "../sftp/sftp_client.h"
#include "../utils/Utility.h"
#include "../utils/Network.h"
#include "../utils/LoopThread.h"
#include "../utils/AppRunner.h"
#include "../utils/DirScanner.h"
#include "../utils/RunLogger.h"

CClientNode::CClientNode(CClientConf* pClientConf, CRunLogger* pLogger)
{
	assert(pClientConf != NULL);
	m_pConfigure = pClientConf;
	m_pLogger = pLogger;
	if(m_pConfigure->nUploadErrorLog != 0)
		m_pLogger->SetErrLogCallback(SendErrLogInfo, this);
	m_nHeartSocket = INVALID_SOCKET;
	m_nAppStartTime = 0;
	m_nCurTaskID = 0;
	m_bNeedRestart = false;
	m_nLastErrCode = MARC_CODE_OK;
	m_nLastTaskFinishedTime = 0;
	m_nLastTaskReqFailTime = 0;
	m_nLastAppVerCheckTime = 0;
	FD_ZERO(&m_fdAllSet);
	memset(&m_stNodeStatus, 0, sizeof(_stClientStatus));
	m_stNodeStatus.nStartupTime = m_pConfigure->nStartupTime;

	//��ʼ��m_pAppRunner
	m_pAppRunner = new CAppRunner;
	assert(m_pAppRunner != NULL);
	m_bAppStarted = false;

	m_pHeartThread = new CLoopThread();
	assert(m_pHeartThread != NULL);
	m_pHeartThread->SetRutine(HeartRutine, this);
	m_nLastHeartTime = 0;
	m_nLastAppStateSendTime = 0;
	m_nLastSourceStatusSendTime = 0;

	m_pTaskThread = new CLoopThread();
	assert(m_pTaskThread != NULL);
	m_pTaskThread->SetRutine(TaskRutine, this);

	m_pAsynUploadThread = new CLoopThread();
	assert(m_pAsynUploadThread != NULL);
	m_pAsynUploadThread->SetRutine(AsynUploadRutine, this);
	INITIALIZE_LOCKER(m_locker);
    m_pResultNodeAddr = NULL;
}

CClientNode::~CClientNode()
{
	m_pLogger->SetErrLogCallback(NULL,NULL);
	if(m_pHeartThread != NULL)
		delete m_pHeartThread;
	if(m_pTaskThread != NULL)
		delete m_pTaskThread;
	if(m_pAsynUploadThread != NULL)
		delete m_pAsynUploadThread;
	if(m_pAppRunner != NULL)
		delete m_pAppRunner;
	DESTROY_LOCKER(m_locker);
    if(m_pResultNodeAddr != NULL)
        free(m_pResultNodeAddr);
}

bool CClientNode::Start(bool bUseBakMaster)
{
	m_nLastErrCode = MARC_CODE_OK;
	m_bNeedRestart = false;
	m_nAppStartTime = 0;
	FD_ZERO(&m_fdAllSet);

	//Master�ڵ��IP���������˿�
	m_sMasterIP = bUseBakMaster?m_pConfigure->sBakMasterIP:m_pConfigure->sMasterIP;
	m_nMasterPort = bUseBakMaster?m_pConfigure->nBakMasterPort:m_pConfigure->nMasterPort;

	//�����ϴ�δ�ϴ��Ľ��ѹ���ļ�
	if(m_pConfigure->nResultUploadEnabled != 0)
	{
		m_pLogger->Write(CRunLogger::LOG_INFO, "Load unfinished result-zipfile...\n");
		LoadUploadResultFile();
	}

	//ע�ᵽMaster�ڵ�	
	int nRetCode = RegisterToMaster();
	if(nRetCode != MARC_CODE_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't register to MasterNode!\n");
		m_nLastErrCode = nRetCode;
		m_bNeedRestart = true;
		return false;
	}
	m_pLogger->Write(CRunLogger::LOG_INFO, "Register to MasterNode successfully!\n");

	//��ʾӦ�ó���汾�Ų�����Ƿ���Ҫ��������
	if(m_pConfigure->nUpdateEnabled != 0)
	{
		m_pLogger->Write(CRunLogger::LOG_INFO, "Current application version is %d\n", m_pConfigure->nCurAppVersion);
		_stAppVerInfo dAppVerInfo;
		if(CheckAppVersion(dAppVerInfo))
			UpdateAppVersion(dAppVerInfo);
		m_nLastAppVerCheckTime = time(0);
	}

	//���������߳�
	if(!m_pHeartThread->Start())
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't start thread! %s:%d\n", __FILE__, __LINE__);
		UnregisterToMaster();
		m_bNeedRestart = true;
		return false;
	}

	//�����������߳�
	if(!m_pTaskThread->Start())
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't start thread! %s:%d\n", __FILE__, __LINE__);
		UnregisterToMaster();
		m_bNeedRestart = true;
		return false;
	}

	//�����첽�ϴ��߳�
	assert(m_pAsynUploadThread != NULL);
	if(m_pConfigure->nAsynUpload == 1)
	{
		 if(!m_pAsynUploadThread->Start())
		 {
			 m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't start thread! %s:%d\n", __FILE__, __LINE__);
			 UnregisterToMaster();
			 m_bNeedRestart = true;
			 return false;
		 }
	}
	
	return true;
}

void CClientNode::Stop()
{
	//ֹͣ�첽�ϴ��߳�
	if(m_pConfigure->nAsynUpload == 1)
	{
		m_pAsynUploadThread->Stop();
	}

	//ֹͣ�����̺߳��������߳�
	m_pHeartThread->Stop();
	m_pTaskThread->Stop();

	//ȡ��ע��
	UnregisterToMaster();

	//���ڵ��������������kill��
	if(m_pAppRunner->IsAppRunning())
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "Application still running, kill it and cleanup input and output folder! %s:%d\n", __FILE__, __LINE__);
		KillApp();
		CleanDir(m_pConfigure->sInputDir.c_str());
		CleanDir(m_pConfigure->sOutputDir.c_str());
	}

	//������δ�ϴ��Ľ���ļ���
	m_pLogger->Write(CRunLogger::LOG_INFO, "Save unfinished result-zipfile...\n");
	SaveUploadResultFiles();
}

void CClientNode::AsynUploadRutine(void *param)
{
	/*****
	* �Ӵ��ϴ��ļ�������ȡ��һ���ļ��ϴ���Result�ڵ�
	* ���ϴ�ʧ���򽫸��ļ��������β���ȴ��Ժ����ϴ�
	*****/
	CClientNode* me = (CClientNode*)param;

	string sResultFile = "";
	LOCK(me->m_locker);
	if(!me->m_oUploadFiles.empty())
	{
		sResultFile = me->m_oUploadFiles.front();
		me->m_oUploadFiles.pop();
	}
	UNLOCK(me->m_locker);

	if(!sResultFile.empty())
	{
		me->m_pLogger->Write(CRunLogger::LOG_INFO, "Upload resule-zipfile %s...\n", sResultFile.c_str());
		if(me->UploadResultFile(sResultFile) != MARC_CODE_OK)
		{
			me->m_pLogger->Write(CRunLogger::LOG_ERROR, "Upload Failed: %s, which will be uploaded later\n", sResultFile.c_str());
			LOCK(me->m_locker);
			me->m_oUploadFiles.push(sResultFile);
			UNLOCK(me->m_locker);
			Sleep(5000);
		}
	}
}

//Client�ڵ�״̬����̣߳��������������ڵ�״̬���͸�Master�ڵ�
void CClientNode::HeartRutine(void* param)
{
	CClientNode* me = (CClientNode*)param;
	if(me->m_nHeartSocket == INVALID_SOCKET) return ;

	//fdset��ʼ��
	fd_set fdRead, fdWrite, fdException;
	memcpy(&fdRead, &me->m_fdAllSet, sizeof(fd_set));
	memcpy(&fdWrite, &me->m_fdAllSet, sizeof(fd_set));
	memcpy(&fdException, &me->m_fdAllSet, sizeof(fd_set));

	//select����
	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;
	int inReady = select((int)me->m_nHeartSocket+1, &fdRead, &fdWrite, &fdException, &timeout);
	if(inReady == 0) //��ʱ
	{
		Sleep(1000);
		return ; 
	}
	if(inReady < 0)  //����
	{
		me->m_pLogger->Write(CRunLogger::LOG_ERROR, "select error: %s! close SOCKET! %s:%d\n", strerror(errno), __FILE__, __LINE__);
		CLOSE_SOCKET(me->m_nHeartSocket);
		me->m_nHeartSocket = INVALID_SOCKET;
		me->m_nLastErrCode = MARC_CODE_SELECT_FAILED;
		me->m_bNeedRestart = true;
		return ; 
	}
	if(FD_ISSET(me->m_nHeartSocket, &fdException)) //�쳣
	{
		CLOSE_SOCKET(me->m_nHeartSocket);
		me->m_nHeartSocket = INVALID_SOCKET;
		me->m_nLastErrCode = MARC_CODE_SOCKET_EXCEPTION;
		me->m_pLogger->Write(CRunLogger::LOG_ERROR, "select exception, maybe closed by MasterNode! %s:%d\n", __FILE__, __LINE__);
		me->m_bNeedRestart = true;
		return ; 
	}
	if(FD_ISSET(me->m_nHeartSocket, &fdRead)) //Master�ڵ�����Ϣ����
	{
		_stDataPacket recvPacket; //���յ�����Ϣ��
		memset(&recvPacket, 0, sizeof(_stDataPacket));
		int nRecev = network_recv(me->m_nHeartSocket, (char*)&recvPacket, sizeof(_stDataPacket));
		switch(nRecev)
		{
		case MARC_NETWORK_OK:			
			//����Master��������Ϣ
			Net2Host(recvPacket);
			me->HandleMasterMsg(recvPacket);
			break;
		case MARC_NETWORK_TIMEOUT: //��ʱ
			me->m_pLogger->Write(CRunLogger::LOG_WARNING, "network_recv timeout! %s:%d\n", __FILE__, __LINE__);
			break;
		case MARC_NETWORK_CLOSED: //���ӱ��ر�
			CLOSE_SOCKET(me->m_nHeartSocket);
			me->m_nHeartSocket = INVALID_SOCKET;
			me->m_nLastErrCode = MARC_CODE_SOCKET_EXCEPTION;
			me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Heartbeat connection closed by MasterNode! %s:%d\n", __FILE__, __LINE__);
			me->m_bNeedRestart = true;
			return ;
		case MARC_NETWORK_ERROR: //�������
			CLOSE_SOCKET(me->m_nHeartSocket);
			me->m_nHeartSocket = INVALID_SOCKET;
			me->m_nLastErrCode = MARC_CODE_SOCKET_EXCEPTION;
			me->m_pLogger->Write(CRunLogger::LOG_ERROR, "Network error: %s! %s:%d\n", strerror(errno), __FILE__, __LINE__);
			me->m_bNeedRestart = true;
			return ;
		default: //Ӧ�ò��ᵽ��
			assert(false);
			break;
		};
	}
	if(FD_ISSET(me->m_nHeartSocket, &fdWrite))
	{
		//����������Ϣ
		if(time(0) - me->m_nLastHeartTime >= me->m_pConfigure->nHeartbeatInterval)
		{
			me->m_nLastHeartTime = (int)time(0);

			//ȡ�õ�ǰ�ڵ�״̬
			_stClientStatus status;
			me->m_stNodeStatus.nCurTaskID = me->m_nCurTaskID;
			me->m_stNodeStatus.nTotalWaitingResults = me->m_oUploadFiles.size();
			memcpy(&status, &me->m_stNodeStatus, sizeof(_stClientStatus));
			Host2Net(status);

			_stDataPacket msgPacket;
			memset(&msgPacket, 0, sizeof(_stDataPacket));
			msgPacket.nCommand = C2L_CMD_HEART_SEND;
			msgPacket.nClientID = me->m_pConfigure->nClientID;
			msgPacket.nBufSize = sizeof(_stClientStatus);
			assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
			memcpy(msgPacket.cBuffer, (char *)&status, sizeof(_stClientStatus));
			Host2Net(msgPacket);
			int iRet = network_send(me->m_nHeartSocket, (char *)&msgPacket, sizeof(_stDataPacket));
			switch(iRet)
			{
			case MARC_NETWORK_OK: //���ͳɹ�
				me->m_pLogger->Write(CRunLogger::LOG_INFO, "Send heartbeat to MasterNode successfully\n");
				break;
			default: //����ʧ��
				//CLOSE_SOCKET(me->m_nHeartSocket);
				//me->m_nHeartSocket = INVALID_SOCKET;
				//me->m_nLastErrCode = MARC_CODE_SEND_FAILED;
				me->m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send heartbeat to MasterNode! %s:%d\n", __FILE__, __LINE__);
				//me->m_bNeedRestart = true;
				break;
			}
		}

#ifndef WIN32
		//���ͽڵ���Դʹ��״����Ϣ
		if(time(0) - me->m_nLastSourceStatusSendTime >= me->m_pConfigure->nSourceStatusInterval)
		{
			me->m_nLastSourceStatusSendTime = (int)time(0);

			//ȡ�õ�ǰ�ڵ���Դʹ��״��
			_stNodeSourceStatus status;
			if(!GetSourceStatusInfo(status))
			{
				me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Can't acquire source status!\n");
			}
			else
			{
				//���͸�Master
				_stDataPacket msgPacket;
				memset(&msgPacket, 0, sizeof(_stDataPacket));
				msgPacket.nCommand = C2L_CMD_SOURCE_STATUS;
				msgPacket.nClientID = me->m_pConfigure->nClientID;
				msgPacket.nBufSize = sizeof(_stNodeSourceStatus);
				assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
				Host2Net(status);
				memcpy(msgPacket.cBuffer, (char *)&status, sizeof(_stNodeSourceStatus));
				Host2Net(msgPacket);
				int iRet = network_send(me->m_nHeartSocket, (char *)&msgPacket, sizeof(_stDataPacket));
				switch(iRet)
				{
				case MARC_NETWORK_OK: //���ͳɹ�
					me->m_pLogger->Write(CRunLogger::LOG_INFO, "Send source status to MasterNode successfully\n");
					break;
				default: //����ʧ��
					//CLOSE_SOCKET(me->m_nHeartSocket);
					//me->m_nHeartSocket = INVALID_SOCKET;
					//me->m_nLastErrCode = MARC_CODE_SEND_FAILED;
					me->m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send source status to MasterNode %s:%d\n", __FILE__, __LINE__);
					//me->m_bNeedRestart = true;
					break;
				}
			}
		}
#endif //WIN32

#if 0
		//����״̬��Ϣ(dgd:���ﲻ������������������)
		if(time(0) - me->m_nLastAppStateSendTime > me->m_pConfigure->nStateInterval)
		{
			me->m_nLastAppStateSendTime = (int)time(0);
			me->m_pLogger->Write(CRunLogger::LOG_INFO, "Send application status to MasterNode...\n");

			//��״̬�ļ���ȡ״̬��Ϣ�����͸�Master�ڵ�
			_stClientState dAppState;
			memset(&dAppState, 0, sizeof(_stClientState));
			if(me->GetAppStateInfo(dAppState))
			{
				_stDataPacket sendPacket;
				memset(&sendPacket, 0, sizeof(_stDataPacket));
				sendPacket.nCommand = C2L_CMD_STATE_SEND;
				sendPacket.nClientID = me->m_pConfigure->nClientID;
				memcpy(sendPacket.cBuffer, (char*)&dAppState, sizeof(_stClientState));
				Host2Net(sendPacket);
				if(network_send(me->m_nHeartSocket, (char*)&sendPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
				{
					me->m_nLastErrCode = MARC_CODE_SEND_FAILED;
					return ; 
				}
			}
			else
			{
			}
		}
#endif
	}
}

void CClientNode::TaskRutine(void* param)
{
	CClientNode *me = (CClientNode*)param;

	//���App�汾�����и�������а汾����
	if(me->m_pConfigure->nUpdateEnabled != 0 && time(0) - me->m_nLastAppVerCheckTime > me->m_pConfigure->nUpdateInterval)
	{
		me->m_nLastAppVerCheckTime = time(0);
		_stAppVerInfo dAppVerInfo;
		if(me->CheckAppVersion(dAppVerInfo))
			me->UpdateAppVersion(dAppVerInfo);
	}

	//�ڵ����δ��������ζ�Žڵ��ڵȴ�������
	if(!me->m_bAppStarted)
	{
		me->OnAppNeedTask();
		return ;
	}

	//�ڵ����������
	assert(me->m_bAppStarted);

	//�жϽڵ�����Ƿ����н���
	if(me->m_pAppRunner->IsAppRunning()) //��������
	{
		//�ж��Ƿ����г�ʱ,��ʱ��kill��
		int nAppRunTime = (int)(time(0) - me->m_nAppStartTime);
		if(me->m_pConfigure->nAppRunTimeout > 0 && nAppRunTime > me->m_pConfigure->nAppRunTimeout)
		{
			me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Application running timeout(%d seconds)! %s:%d\n", nAppRunTime, __FILE__, __LINE__);
			me->OnAppTimeout(nAppRunTime);
		}
		else
		{
			me->OnAppRunning();
		}
		Sleep(2000);
		return ;
	}

	//�жϳ����Ƿ�ִ�гɹ���ִ�гɹ��ĳ���Ӧ�ڵ�ǰĿ¼������.success�ļ���
	time_t nTimeUsed = time(0) - me->m_nAppStartTime;
	string sAppFlagFile = ".success";
	if(DIR_EXIST(sAppFlagFile.c_str()))
	{
		me->m_pLogger->Write(CRunLogger::LOG_INFO, "Application running finished normally, %d seconds.\n", nTimeUsed);
		deleteFile(sAppFlagFile.c_str());
		me->m_nLastTaskFinishedTime = time(0);
		me->OnAppFinished();
		me->m_stNodeStatus.nTotalFinishedTasks++;
		me->m_stNodeStatus.nTotalExecTimeUsed += nTimeUsed;

		//���input output�ļ���
		CleanDir(me->m_pConfigure->sInputDir.c_str());
		CleanDir(me->m_pConfigure->sOutputDir.c_str());
		me->m_nAppStartTime = 0;
		me->m_bAppStarted  = false;
	}
	else //����������
	{
		me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Application running finished abnormally (.success not found)! %s:%d\n", __FILE__, __LINE__);
		me->OnAppFailed();
	}
	
	Sleep(1000);
	return ;
}

bool CClientNode::CheckAppVersion(_stAppVerInfo &dAppVerInfo)
{
	//���ӵ�Master�ڵ��������
	m_pLogger->Write(CRunLogger::LOG_INFO, "Request to MasterNode for Application version update...\n");
	SOCKET nSocket = network_connect(m_sMasterIP.c_str(), m_nListenerPort);
	if(nSocket == INVALID_SOCKET) 
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't connect to MasterNode! %s:%d\n", __FILE__, __LINE__);
		return false;
	}

	//��Master�ڵ㷢��APP�汾��������
	_stDataPacket msgPacket;
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = C2L_CMD_APPVER_REQ;
	msgPacket.nClientID = m_pConfigure->nClientID;
	msgPacket.nOffset = m_pConfigure->nCurAppVersion;
	msgPacket.nBufSize = (int)m_pConfigure->sAppType.length();
	assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
	strcpy(msgPacket.cBuffer, m_pConfigure->sAppType.c_str());
	Host2Net(msgPacket);
	if(network_send(nSocket, (char*)&msgPacket, sizeof(_stDataPacket))!=MARC_NETWORK_OK) 
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send C2L_CMD_APPVER_REQ message! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return false;
	}
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	if(network_recv(nSocket, (char*)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to receive reponse message! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return false;
	}
	Net2Host(msgPacket);

	//�����Ӧ��Ϣ
	bool bNeedUpdate = false;
	switch(msgPacket.nCommand)
	{
	case L2C_CMD_APPVER_YES:
		memcpy(&dAppVerInfo, msgPacket.cBuffer, sizeof(_stAppVerInfo));
		Net2Host(dAppVerInfo);
		bNeedUpdate = true;
		m_pLogger->Write(CRunLogger::LOG_INFO, "Application needs to be updated, The newest version is %d\n", dAppVerInfo.nUpdateVersion);
		break;
	case L2C_CMD_APPVER_NO:
		m_pLogger->Write(CRunLogger::LOG_INFO, "Application doesn't need to be updated\n");
		break;
	default:
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid message command: %d, %s:%d\n", msgPacket.nCommand, __FILE__, __LINE__);
		break;
	};

	//�ر���Master�ڵ�������������
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = C2L_CMD_CLOSE;
	msgPacket.nClientID = m_pConfigure->nClientID;
	msgPacket.nBufSize = (int)m_pConfigure->sAppType.length();
	assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
	strcpy(msgPacket.cBuffer, m_pConfigure->sAppType.c_str());
	Host2Net(msgPacket);
	network_send(nSocket, (char *)&msgPacket, sizeof(_stDataPacket));
	//Sleep(1000);
	CLOSE_SOCKET(nSocket);

	return bNeedUpdate;
}

void CClientNode::UpdateAppVersion(const _stAppVerInfo &dAppVerInfo)
{
	//����������
	m_pLogger->Write(CRunLogger::LOG_INFO, "Download update-zipfile from MasterNode (file path: %s) ...\n", dAppVerInfo.chUpdateFile);
	time_t nStartTime = time(0);
	string sLocalZipFile = m_pConfigure->sZipUpdateDir + getFileName(dAppVerInfo.chUpdateFile, true);
	int nRetCode = sftp_client_download_file(m_sMasterIP.c_str(), dAppVerInfo.usPort, dAppVerInfo.chUpdateFile, sLocalZipFile.c_str());
	unsigned int nTimeUsed = (unsigned int)(time(0) - nStartTime);
	switch(nRetCode)
	{
	case SFTP_CODE_OK: //���سɹ�
		m_pLogger->Write(CRunLogger::LOG_INFO, "update-zipfile downloaded successfully (version=%d, file=%s), %d seconds\n", 
			dAppVerInfo.nUpdateVersion, sLocalZipFile.c_str(), nTimeUsed);
		break;
	default: //��������
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to download update-zipfile(version=%d, File=%s), ErrCode: %d, %s:%d\n",
			dAppVerInfo.nUpdateVersion, sLocalZipFile.c_str(), nRetCode, __FILE__, __LINE__);
		return ;
	}

	//��ѹ������
	char chCmd[1024] = {0};
#ifdef WIN32
	sprintf(chCmd, "%s unzip %s %s", MARC_MYZIP_APPCMD, sLocalZipFile.c_str(), m_pConfigure->sUpdateTargetPath.c_str());
#else
	sprintf(chCmd, "/bin/tar -xzf %s -C %s ./", sLocalZipFile.c_str(), m_pConfigure->sUpdateTargetPath.c_str());
#endif
	m_pLogger->Write(CRunLogger::LOG_INFO, "unzip update-zipfile: %s\n", chCmd);
	if(!Exec(chCmd))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "unzip failed: %s\n", chCmd);
		return ;
	}

	//���浱ǰ�汾��
	m_pConfigure->nCurAppVersion = dAppVerInfo.nUpdateVersion;
	WriteAppVersion(m_pConfigure->sAppVerFile, m_pConfigure->sAppType, m_pConfigure->nCurAppVersion);
	m_pLogger->Write(CRunLogger::LOG_INFO, "Application updated successfully! New version is %d\n", m_pConfigure->nCurAppVersion);
}

void CClientNode::OnAppFailed()
{
	//���input��output�ļ���
	m_pLogger->Write(CRunLogger::LOG_INFO, "Application running failed, cleanup input and output folder\n");
	CleanDir(m_pConfigure->sInputDir.c_str());
	CleanDir(m_pConfigure->sOutputDir.c_str());

	//������Ϣ��֪Master��
	_stDataPacket msgPacket;
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = C2L_CMD_APP_FAILED;
	msgPacket.nClientID = m_pConfigure->nClientID;
	msgPacket.nOffset = m_nCurTaskID;
	msgPacket.nBufSize = (int)m_pConfigure->sAppType.length();
	assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
	strcpy(msgPacket.cBuffer, m_pConfigure->sAppType.c_str());
	Host2Net(msgPacket);
	if(network_send(m_nHeartSocket, (char *)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send C2L_CMD_APP_FAILED message to MasterNode! %s:%d\n", __FILE__, __LINE__);
		m_nLastErrCode = MARC_CODE_SEND_FAILED;
	}

	m_nAppStartTime = 0;
	m_nCurTaskID = 0;
	m_bAppStarted  = false;

	//��¼״̬
	m_stNodeStatus.nTotalFailedTasks++;
}

void CClientNode::OnAppRunning()
{
	m_pLogger->Write(CRunLogger::LOG_INFO, "Application(AppType=%s) is running...\n", m_pConfigure->sAppType.c_str());
}

void CClientNode::OnAppTimeout(int nAppRunTime)
{
	m_pLogger->Write(CRunLogger::LOG_WARNING, "Kill Application and cleanup input and output folder...\n");
	KillApp();
	CleanDir(m_pConfigure->sInputDir.c_str());
	CleanDir(m_pConfigure->sOutputDir.c_str());

	//������Ϣ��֪Master��
	_stDataPacket msgPacket;
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = C2L_CMD_APP_TIMEOUT;
	msgPacket.nClientID = m_pConfigure->nClientID;
	msgPacket.nOffset = m_nCurTaskID;
	msgPacket.nBufSize = (int)m_pConfigure->sAppType.length();
	assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
	strcpy(msgPacket.cBuffer, m_pConfigure->sAppType.c_str());
	Host2Net(msgPacket);
	if(network_send(m_nHeartSocket, (char *)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send C2L_CMD_APP_TIMEOUT message to MasterNode! %s:%d\n", __FILE__, __LINE__);
		m_nLastErrCode = MARC_CODE_SEND_FAILED;
	}

	m_stNodeStatus.nTotalFailedTasks++;
	m_nAppStartTime = 0;
	m_nCurTaskID = 0;
	m_bAppStarted  = false;
}

int CClientNode::RegisterToMaster()
{
	int nRetCode = MARC_CODE_OK;
	m_pLogger->Write(CRunLogger::LOG_INFO, "Register to MasterNode(%s:%d) ...\n", m_sMasterIP.c_str(), m_nMasterPort);

	//���Master�ڵ�ĴӼ����˿�
	m_pLogger->Write(CRunLogger::LOG_INFO, "Accquire listener port from MasterNode...\n");
	nRetCode = GetListenerPort(m_nListenerPort);
	if(nRetCode != MARC_CODE_OK)
	{
		return nRetCode;
	}	

	//����Master�ڵ�ĴӼ�������
	//��ע������ע���õ�SOCKET��һֱ���֣������������ͺ�״̬��Ϣ����
	m_pLogger->Write(CRunLogger::LOG_INFO, "Connect to Listener(%s:%d) of MasterNode...\n", m_sMasterIP.c_str(), m_nListenerPort);
	m_nHeartSocket = network_connect(m_sMasterIP.c_str(), m_nListenerPort);
	if(m_nHeartSocket == INVALID_SOCKET)
	{
		m_pLogger->Write(CRunLogger::LOG_INFO, "Can't connect to listener of MasterNode! %s:%d\n", __FILE__, __LINE__);
		return MARC_CODE_CONNECT_FAILED;
	}

	//�����������ע��������Ϣ��Client�ڵ������б�ʾMARC_MAGIC_NUMBER,Master���ϣ�
	_stDataPacket msgPacket;
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = C2L_CMD_REGISTER_REQ;
	msgPacket.nOffset = MARC_MAGIC_NUMBER;
	msgPacket.nClientID = m_pConfigure->nClientID;
	assert(m_pConfigure->sAppType.length() + m_pConfigure->sInstallPath.length() <= MARC_MAX_MSG_BYTES);
	//strcpy(msgPacket.cBuffer, m_pConfigure->sAppType.c_str());
	_snprintf(msgPacket.cBuffer, MARC_MAX_MSG_BYTES, "%s:%s", m_pConfigure->sAppType.c_str(), m_pConfigure->sInstallPath.c_str());
	Host2Net(msgPacket);
	if(network_send(m_nHeartSocket, (char *)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		CLOSE_SOCKET(m_nHeartSocket);
		m_nHeartSocket = INVALID_SOCKET;
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send C2L_CMD_REGISTER_REQ message to MasterNode! %s:%d\n", __FILE__, __LINE__);
		return MARC_CODE_SEND_FAILED;
	}

	//����ע��Ӧ����Ϣ
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	if(network_recv(m_nHeartSocket, (char*)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		CLOSE_SOCKET(m_nHeartSocket);
		m_nHeartSocket = INVALID_SOCKET;
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to receive response message of C2L_CMD_REGISTER_REQ from MasterNode! %s:%d\n", __FILE__, __LINE__);
		return MARC_CODE_RECV_FAILED;
	}
	Net2Host(msgPacket);

	//���ݲ�ͬ��ע���������ж�
	switch(msgPacket.nCommand)
	{
	case L2C_CMD_REGISTER_YES: //ע��ɹ�
		break;
	case L2C_CMD_REGISTER_NO: //ע��ʧ��
		CLOSE_SOCKET(m_nHeartSocket);
		m_nHeartSocket = INVALID_SOCKET;
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Refused to register to MasterNode! %s:%d\n", __FILE__, __LINE__);
		return MARC_CODE_REGISTER_REFUSED;
	case L2C_CMD_INVALID_CLIENT: //�Ƿ�Client�ڵ�
		CLOSE_SOCKET(m_nHeartSocket);
		m_nHeartSocket = INVALID_SOCKET;
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid ClientNode detected by MasterNode! %s:%d\n", __FILE__, __LINE__);
		return MARC_CODE_REGISTER_REFUSED;
	default: //��������Ϸ�
		CLOSE_SOCKET(m_nHeartSocket);
		m_nHeartSocket = INVALID_SOCKET;
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid message command(%d) received from MasterNode! %s:%d\n", msgPacket.nCommand, __FILE__, __LINE__);
		return MARC_CODE_INVALID_COMMAND;
	}

	FD_SET(m_nHeartSocket, &m_fdAllSet);
	return MARC_CODE_OK;
}

void CClientNode::UnregisterToMaster()
{
	if(m_nHeartSocket != INVALID_SOCKET)
	{
		//�Ͽ���Master�ڵ������
		_stDataPacket msgPacket;
		memset(&msgPacket, 0, sizeof(_stDataPacket));
		msgPacket.nCommand = C2L_CMD_UNREGISTER;
		msgPacket.nClientID = m_pConfigure->nClientID;
		msgPacket.nBufSize = (int)m_pConfigure->sAppType.length();
		assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
		strcpy(msgPacket.cBuffer, m_pConfigure->sAppType.c_str());
		Host2Net(msgPacket);
		network_send(m_nHeartSocket, (char*)&msgPacket, sizeof(_stDataPacket));
		CLOSE_SOCKET(m_nHeartSocket);
		m_nHeartSocket = INVALID_SOCKET;
	}
}

void CClientNode::HandleMasterMsg(const _stDataPacket &msgPacket)
{
	//ĿǰMaster�ڵ㲻��ͨ���������Ӹ�Client�ڵ㷢��Ϣ
	m_pLogger->Write(CRunLogger::LOG_INFO, "Receive message(command:%d) from MasterNode.\n", msgPacket.nCommand);
}

int CClientNode::GetListenerPort(unsigned short& nListenerPort)
{
	//����Master�ڵ�
	SOCKET nClientSock = network_connect(m_sMasterIP.c_str(), m_nMasterPort);
	if(nClientSock == INVALID_SOCKET)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't connect to MasterNode(%s:%d)! %s:%d\n", m_sMasterIP.c_str(), m_nMasterPort, __FILE__, __LINE__);
		return MARC_CODE_CONNECT_FAILED;
	}

	//����ע��������Ϣ(������б�ʾMARC_MAGIC_NUMBER, Master�˲���)
	_stDataPacket msgPacket;
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = C2M_CMD_REGISTER_REQ;
	msgPacket.nClientID = m_pConfigure->nClientID;
	msgPacket.nOffset = MARC_MAGIC_NUMBER; 
	assert(m_pConfigure->sAppType.length() + m_pConfigure->sInstallPath.length() <= MARC_MAX_MSG_BYTES);	
	//strcpy(msgPacket.cBuffer, m_pConfigure->sAppType.c_str());
	_snprintf(msgPacket.cBuffer, MARC_MAX_MSG_BYTES, "%s:%s", m_pConfigure->sAppType.c_str(), m_pConfigure->sInstallPath.c_str());
	Host2Net(msgPacket);
	if(network_send(nClientSock, (char *)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send C2M_CMD_REGISTER_REQ message to MasterNode, %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nClientSock);
		return MARC_CODE_SEND_FAILED;
	}

	//���ղ�����ע��Ӧ����Ϣ
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	if(network_recv(nClientSock, (char*)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to receive response message of C2M_CMD_REGISTER_REQ from MasterNode! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nClientSock);
		return MARC_CODE_RECV_FAILED;
	}
	int nRetCode = MARC_CODE_OK;
	Net2Host(msgPacket);
	switch(msgPacket.nCommand)
	{
	case M2C_CMD_REGISTER_YES://ע��ɹ�,�õ�ʵ�ʵĴӼ��������IP�Ͷ˿�
		nListenerPort = msgPacket.nOffset;

		if(msgPacket.nClientID > 0)
		{
			assert(m_pConfigure->nClientID == 0);
			m_pConfigure->nClientID = msgPacket.nClientID;
			m_pLogger->Write(CRunLogger::LOG_INFO, "Assigned ClientID=%d\n", m_pConfigure->nClientID);
		}
		break;
	case M2C_CMD_REGISTER_NO: //ע��ʧ��
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Refused to register to MasterNode! %s:%d\n", __FILE__, __LINE__);
		nRetCode = MARC_CODE_REGISTER_REFUSED;
		break;
	default: //��Ч����
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid message command(%d) received from MasterNode! %s:%d\n", msgPacket.nCommand, __FILE__, __LINE__);
		nRetCode = MARC_CODE_INVALID_COMMAND;
		break;
	};

	//�ر���Master�ڵ�������������
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = C2M_CMD_CLOSE;
	msgPacket.nClientID = m_pConfigure->nClientID;
	msgPacket.nBufSize = (int)m_pConfigure->sAppType.length();
	assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
	strcpy(msgPacket.cBuffer, m_pConfigure->sAppType.c_str());
	Host2Net(msgPacket);
	network_send(nClientSock, (char *)&msgPacket, sizeof(_stDataPacket));
	//Sleep(1000);
	CLOSE_SOCKET(nClientSock);

	return nRetCode;
}


int CClientNode::GetTaskInfo(_stTaskReqInfo &task)
{
	//����Master��������
	SOCKET nSocket = network_connect(m_sMasterIP.c_str(), m_nListenerPort);
	if(nSocket == INVALID_SOCKET) 
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't connect to MasterNode! %s:%d\n", __FILE__, __LINE__);
		m_bNeedRestart = true;
		return MARC_CODE_CONNECT_FAILED;
	}

	//��������������Ϣ
	_stDataPacket stSendPack;
	memset(&stSendPack,0,sizeof(_stDataPacket));
	stSendPack.nCommand = C2L_CMD_TASK_REQ;
	stSendPack.nClientID = m_pConfigure->nClientID;
	stSendPack.nBufSize = (int)m_pConfigure->sAppType.length();
	assert(stSendPack.nBufSize <= MARC_MAX_MSG_BYTES);
	strcpy(stSendPack.cBuffer, m_pConfigure->sAppType.c_str());
	Host2Net(stSendPack);
	if(network_send(nSocket, (char*)&stSendPack, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send C2L_CMD_TASK_REQ message to MasterNode! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return MARC_CODE_SEND_FAILED;
	}

	//������������Ӧ����Ϣ
	_stDataPacket stRecvPack;
	memset(&stRecvPack, 0, sizeof(_stDataPacket));
	if(network_recv(nSocket, (char*)&stRecvPack, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to receive response message of C2L_CMD_TASK_REQ from MasterNode! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return MARC_CODE_RECV_FAILED;
	}
	Net2Host(stRecvPack);

	//����Ӧ�����������Ӧ����
	int nRetCode = MARC_CODE_OK;
	bool bNeedRestart = false;
	switch(stRecvPack.nCommand)
	{
	case L2C_CMD_TASK_YES: //������
		memcpy(&task, stRecvPack.cBuffer, sizeof(_stTaskReqInfo));
		Net2Host(task);
		nRetCode = MARC_CODE_OK;
		break;
	case L2C_CMD_TASK_NO: //������Ҳ������Result�ڵ�û������
		nRetCode = MARC_CODE_NOTASK;
		break;
	case L2C_CMD_INVALID_CLIENT: //Client��Ч
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid ClientNode detected by MasterNode, maybe deleted from node queue by MasterNode for network exception! %s:%d\n", __FILE__, __LINE__);
		nRetCode = MARC_CODE_INVALID_CLIENT;
		bNeedRestart = true;
		break;
	default: //�Ƿ�����
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid message command(%d) received from MasterNode! %s:%d\n", stRecvPack.nCommand, __FILE__, __LINE__);
		nRetCode = MARC_CODE_INVALID_COMMAND;
		break;
	};

	//�Ͽ���������
	memset(&stSendPack,0,sizeof(_stDataPacket));
	stSendPack.nCommand = C2L_CMD_CLOSE;
	stSendPack.nClientID = m_pConfigure->nClientID;
	stSendPack.nBufSize = (int)m_pConfigure->sAppType.length();
	assert(stSendPack.nBufSize <= MARC_MAX_MSG_BYTES);
	strcpy(stSendPack.cBuffer, m_pConfigure->sAppType.c_str());
	Host2Net(stSendPack);
	network_send(nSocket, (char*)&stSendPack, sizeof(_stDataPacket));
	//Sleep(1000);
	CLOSE_SOCKET(nSocket);
	m_bNeedRestart = bNeedRestart;

	return nRetCode;
}

int CClientNode::GetResultNodeAddr(_stResultNodeAddr &rstSvrAddr)
{
	//����Master��������
	SOCKET nSocket = network_connect(m_sMasterIP.c_str(), m_nListenerPort);
	if(nSocket == INVALID_SOCKET)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't connect to MasterNode! %s:%d\n", __FILE__, __LINE__);
		m_bNeedRestart = true;
		return MARC_CODE_CONNECT_FAILED;
	}

	//�����ϴ�������Ϣ�������
	_stDataPacket msgPacket;
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = C2L_CMD_UPLOAD_REQ;
	msgPacket.nClientID = m_pConfigure->nClientID;
	msgPacket.nBufSize = (int)m_pConfigure->sAppType.length();
	assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
	strcpy(msgPacket.cBuffer, m_pConfigure->sAppType.c_str());
	Host2Net(msgPacket);
	if(network_send(nSocket, (char*)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK) 
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send C2L_CMD_UPLOAD_REQ message to MasterNode! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return MARC_CODE_SEND_FAILED;
	}

	//�����ϴ�����Ӧ����Ϣ
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	if(network_recv(nSocket, (char*)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to receive response message of C2L_CMD_UPLOAD_REQ from MasterNode! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return MARC_CODE_RECV_FAILED;
	}
	Net2Host(msgPacket);

	//����Ӧ������������Ӧ����
	int nRetCode = MARC_CODE_OK;
	bool bNeedRestart = false;
	switch(msgPacket.nCommand)
	{
	case L2C_CMD_UPLOAD_YES:
		memcpy(&rstSvrAddr, msgPacket.cBuffer, sizeof(_stResultNodeAddr));
		Net2Host(rstSvrAddr);
		break;
	case L2C_CMD_UPLOAD_NO:
		m_pLogger->Write(CRunLogger::LOG_WARNING, "L2C_CMD_UPLOAD_NO received from MasterNode, maybe ResultNode not found! %s:%d\n", __FILE__, __LINE__);
		nRetCode = MARC_CODE_NO_RESULT_SERVER;
		break;
	case L2C_CMD_INVALID_CLIENT: //Client��Ч
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid ClientNode detected by MasterNode, maybe deleted from node queue by MasterNode for network exception! %s:%d\n", __FILE__, __LINE__);
		nRetCode = MARC_CODE_INVALID_CLIENT;
		bNeedRestart = true;
		break;
	default:
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid message command(%d) received from MasterNode! %s:%d\n", msgPacket.nCommand, __FILE__, __LINE__);
		nRetCode = MARC_CODE_INVALID_COMMAND;
		break;
	};

	//�Ͽ���������
	memset(&msgPacket,0,sizeof(_stDataPacket));
	msgPacket.nCommand = C2L_CMD_CLOSE;
	msgPacket.nClientID = m_pConfigure->nClientID;
	msgPacket.nBufSize = (int)m_pConfigure->sAppType.length();
	assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
	strcpy(msgPacket.cBuffer, m_pConfigure->sAppType.c_str());
	Host2Net(msgPacket);
	network_send(nSocket, (char*)&msgPacket, sizeof(_stDataPacket));
	//Sleep(1000);
	CLOSE_SOCKET(nSocket);
	m_bNeedRestart = bNeedRestart;

	return nRetCode;
}

void CClientNode::OnAppFinished()
{
	//������Ϣ��֪Master�ڵ�
	_stDataPacket msgPacket;
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = C2L_CMD_APP_FINISHED;
	msgPacket.nClientID = m_pConfigure->nClientID;
	msgPacket.nOffset = m_nCurTaskID;
	msgPacket.nBufSize = (int)m_pConfigure->sAppType.length();
	assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
	strcpy(msgPacket.cBuffer, m_pConfigure->sAppType.c_str());
	Host2Net(msgPacket);
	if(network_send(m_nHeartSocket, (char *)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send C2L_CMD_APP_FINISHED message to MasterNode! %s:%d\n", __FILE__, __LINE__);
		m_nLastErrCode = MARC_CODE_SEND_FAILED;
		return ;
	}

	if(m_pConfigure->nResultUploadEnabled == 0)
	{
		m_pLogger->Write(CRunLogger::LOG_INFO, "Needn't upload result data (ResultUploadEnabled=0).\n");
		return ;
	}

	//�����Ŀ¼�Ƿ�������
	CDirScanner ds(m_pConfigure->sOutputDir.c_str(),false);
	const vector<string>& oFileList = ds.GetAllList();
	if(oFileList.empty())
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "Any data file not found in result folder %s! %s:%d\n",
			m_pConfigure->sOutputDir.c_str(), __FILE__, __LINE__);
		return ;
	}

	//���ϵͳʱ�䣬��ʽYYYYMMDDhhmmss
	string sCurTime = formatDateTime(time(0), 1);

	//���ѹ���ļ�·��(��./result/1_NewsGather_result_20100204095923.myzip)
	char sResultFilePath[512] = {0};
	sprintf(sResultFilePath, "%s%d_%s_result_%s.myzip",
		m_pConfigure->sZipResultDir.c_str(),
		m_pConfigure->nClientID, 
		m_pConfigure->sAppType.c_str(), 
		sCurTime.c_str());

	//ִ��ѹ������
	char chCmd[256] = {0};
#ifdef WIN32
	sprintf(chCmd,"%s zip %s %s", MARC_MYZIP_APPCMD, m_pConfigure->sOutputDir.c_str(), sResultFilePath);		
#else
	sprintf(chCmd, "/bin/tar -C %s ./ -czf %s", m_pConfigure->sOutputDir.c_str(), sResultFilePath);	
#endif
	m_pLogger->Write(CRunLogger::LOG_INFO, "myzip result folder: [%s] -> %s\n", m_pConfigure->sOutputDir.c_str(), sResultFilePath);
	if(!Exec(chCmd))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "myzip failed and exit: %s [%s:%d]\n", chCmd, __FILE__, __LINE__);
		Sleep(1000);
		_exit(-1);
	}

	//���ѹ������ļ��Ƿ����
	if(!DIR_EXIST(sResultFilePath))
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "result-zipfile not found: %s, maybe empty src or myzip failed. [%s:%d]\n", sResultFilePath, __FILE__, __LINE__);
		return ;
	}

	//������ϴ���Result�ڵ�
	if(m_pConfigure->nAsynUpload == 1)
	{
		//�첽�ϴ���ʽ�����ϴ��Ľ���ļ���������ϴ��ļ�����
		m_pLogger->Write(CRunLogger::LOG_INFO, "Asynchronous upload: %s\n", sResultFilePath);
		LOCK(m_locker);
		m_oUploadFiles.push(sResultFilePath);
		UNLOCK(m_locker);
	}
	else
	{
		m_pLogger->Write(CRunLogger::LOG_INFO, "Upload result-zipfile: %s ...\n", sResultFilePath);
		int nRetCode = UploadResultFile(sResultFilePath);
		if(nRetCode != MARC_CODE_OK)
		{
			m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to upload result-zipfile: %s, %s:%d\n", sResultFilePath, __FILE__, __LINE__);
			m_nLastErrCode = nRetCode;

			//�����ϴ�ʧ�ܵ��ļ��Ա��´�ϵͳ�������������ϴ�
			LOCK(m_locker);
			m_oUploadFiles.push(sResultFilePath);
			UNLOCK(m_locker);
		}
	}
}

int CClientNode::UploadResultFile(const string& sResultFilePath)
{
	int nRetCode = MARC_CODE_OK;
	if(!DIR_EXIST(sResultFilePath.c_str()))
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "result-zipfile not found: %s\n", sResultFilePath.c_str());
		return MARC_CODE_OK;
	}

	/*
	RememberResultAddr��0ʱ��Client�ڵ���¼�״��ϴ��������ʱ��Master����õ���Result�ڵ��ַ��
	�˺󽫼���ʹ�ø�Result�ڵ����ϴ�������ݣ�����ĳ���ϴ�ʧ�ܣ���
	RememberResultAddrΪ0ʱ��Client�ڵ�ÿ���ϴ��������ʱ������Master����õ�Result�ڵ��ַ��
	*/

	if(m_pConfigure->nRememberResultAddr==0 && m_pResultNodeAddr!=NULL)
	{
		free(m_pResultNodeAddr);
		m_pResultNodeAddr = NULL;
	}

    if(m_pResultNodeAddr == NULL)
    {
	    m_pResultNodeAddr = (_stResultNodeAddr*)malloc(sizeof(_stResultNodeAddr));
        assert(m_pResultNodeAddr != NULL);
    	memset(m_pResultNodeAddr, 0, sizeof(_stResultNodeAddr));
    	m_pLogger->Write(CRunLogger::LOG_INFO, "Request ResultNode address from MasterNode ...\n");
	    nRetCode = GetResultNodeAddr(*m_pResultNodeAddr);
	    if(nRetCode != MARC_CODE_OK)
        {
		    m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't accquire ResultNode address! ErrCode: %d, %s:%d\n", nRetCode, __FILE__, __LINE__);
            free(m_pResultNodeAddr);
            m_pResultNodeAddr = NULL;
			m_stNodeStatus.nTotalFailedResults++;
            return nRetCode;
        }
    }

    assert(m_pResultNodeAddr != NULL);
	m_pLogger->Write(CRunLogger::LOG_INFO, "ResultNode address accquired (%s:%d|%s)\n", m_pResultNodeAddr->chIp, m_pResultNodeAddr->usPort, m_pResultNodeAddr->chSavePath);
	string sRemoteResultFilePath = string(m_pResultNodeAddr->chSavePath) + getFileName(sResultFilePath, true); //Result�ڵ���ļ�·��
	time_t nStartTime = time(0);
	int nSftpCode = sftp_client_upload_file(m_pResultNodeAddr->chIp, m_pResultNodeAddr->usPort, sResultFilePath.c_str(), sRemoteResultFilePath.c_str());
	if(nSftpCode != SFTP_CODE_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "sftp_client_upload_file failed! ErrCode: %d, %s:%d\n", nSftpCode, __FILE__, __LINE__);
        free(m_pResultNodeAddr);
        m_pResultNodeAddr = NULL;
		m_stNodeStatus.nTotalFailedResults++;
		nRetCode = MARC_CODE_UPLOAD_FILE_FAILED;
	}
	else
	{
		time_t nTimeUsed = time(0) - nStartTime;
		m_stNodeStatus.nTotalFinishedResults++;
		m_stNodeStatus.nTotalUploadedTimeUsed += nTimeUsed;
		m_pLogger->Write(CRunLogger::LOG_INFO, "result-zipfile uploaded successfully (%d seconds): %s\n",  nTimeUsed, sResultFilePath.c_str());
		if(m_pConfigure->nAutoDeleteResultFile != 0)
		{
			m_pLogger->Write(CRunLogger::LOG_INFO, "Auto delete the result-zipfile: %s\n", sResultFilePath.c_str());
			deleteFile(sResultFilePath.c_str());
		}
	}
	return nRetCode;
}

void CClientNode::OnAppNeedTask()
{
	m_nCurTaskID = 0;
	if(m_pConfigure->nAsynUpload == 1)
	{
		//�첽�ϴ���ʽ�£������ϴ��ļ�������������ٻ�ȡ����
		if((int)m_oUploadFiles.size() > m_pConfigure->nMaxWaitingUploadFiles)
		{
			m_pLogger->Write(CRunLogger::LOG_WARNING, "Asynchronous upload files exceed %d, waiting ... %s:%d\n",
				m_pConfigure->nMaxWaitingUploadFiles, __FILE__, __LINE__);
			Sleep(2000);
			return ;
		}
	}
	else
	{
		//ͬ���ϴ���ʽ�£���ȴ����д��ϴ��ļ������ϴ���ϲŻ�ȡ����
		LOCK(m_locker);
		while(!m_oUploadFiles.empty())
		{
			string sResultFile = m_oUploadFiles.front();
			m_oUploadFiles.pop();
			m_pLogger->Write(CRunLogger::LOG_INFO, "Upload result-zipfile: %s ...\n", sResultFile.c_str());
			if(UploadResultFile(sResultFile) != MARC_CODE_OK)
			{
				m_oUploadFiles.push(sResultFile);
				m_pLogger->Write(CRunLogger::LOG_ERROR, "Upload failed: %s, which will be uploaded later.\n", sResultFile.c_str());
				Sleep(2000);
				break;
			}
		}
		UNLOCK(m_locker);
		if(!m_oUploadFiles.empty())
		{
			Sleep(2000);
			return;
		}
	}

	//δ��������������������ļ�����������������
	if(m_pConfigure->sAppCmd.empty() || !DIR_EXIST(m_pConfigure->sAppCmdFile.c_str()))
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "AppCmd file is not specified or not existed: %s\n", m_pConfigure->sAppCmdFile.c_str());
		Sleep(1000);
		return ;
	}

	//�ϴ���������ʧ��ʱ����뵱ǰʱ��Ͻ�ʱ��������������
	if(time(0) - m_nLastTaskReqFailTime < m_pConfigure->nTaskReqTimeInterval)
	{
		Sleep(1000);
		return ;
	}

	//�ϴ��������ʱ����뵱ǰʱ��Ͻ���������������
	if(time(0) - m_nLastTaskFinishedTime < m_pConfigure->nTaskReqWaitTime)
	{
		Sleep(1000);
		return ;
	}

	//ȡ��������Ϣ
	m_pLogger->Write(CRunLogger::LOG_INFO, "Request task from MasterNode ...\n");
	_stTaskReqInfo task;
	int nRetCode = GetTaskInfo(task);
	switch(nRetCode)
	{
	case MARC_CODE_OK:
		m_pLogger->Write(CRunLogger::LOG_INFO, "Task accquired(%s:%d|%s, TaskID=%d), begin to download ...\n",
			m_sMasterIP.c_str(), task.usPort, task.chTaskFile, task.nTaskID);
		break;
	case MARC_CODE_NOTASK:
		m_pLogger->Write(CRunLogger::LOG_INFO, "No task found on MasterNode\n");
		m_nLastTaskReqFailTime = (int)time(0);
		Sleep(1000);
		return ;
	default: //�������
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't accquire task(ErrCode: %d) %s:%d\n", nRetCode, __FILE__, __LINE__);
		m_nLastErrCode = nRetCode;
		m_nLastTaskReqFailTime = (int)time(0);
		return ;
	};

	//��������ѹ���ļ�
	time_t nStartTime = time(0);
	string sLocalTaskZipFilePath = m_pConfigure->sZipTaskDir + getFileName(task.chTaskFile, true); //��������ѹ���ļ�·��
	nRetCode = sftp_client_download_file(m_sMasterIP.c_str(), task.usPort, task.chTaskFile, sLocalTaskZipFilePath.c_str());
	unsigned int nTimeUsed = (unsigned int)(time(0) - nStartTime);
	switch(nRetCode)
	{
	case SFTP_CODE_OK: //���سɹ�
		m_pLogger->Write(CRunLogger::LOG_INFO, "Task downloaded successfully(TaskID=%d, TaskFile=%s), %d seconds\n", 
			task.nTaskID, sLocalTaskZipFilePath.c_str(), nTimeUsed);
		m_nCurTaskID = task.nTaskID;
		m_stNodeStatus.nLastFetchedTime = (unsigned int)time(0);
		m_stNodeStatus.nTotalFetchedTasks++;
		m_stNodeStatus.nTotalFecthedTimeUsed += nTimeUsed;
		strcpy(m_stNodeStatus.sLastFetchedFile, sLocalTaskZipFilePath.c_str());
		OnTaskDownloaded(task.nTaskID, sLocalTaskZipFilePath);
		break;
	default: //��������
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to download the task(TaskID=%d, TaskFile=%s), ErrCode: %d, %s:%d\n",
			task.nTaskID, sLocalTaskZipFilePath.c_str(), nRetCode, __FILE__, __LINE__);
		OnTaskDownloadFailed(task.nTaskID, sLocalTaskZipFilePath);
		m_nLastErrCode = MARC_CODE_DOWNLOAD_FILE_FAILED;
		//m_nLastTaskReqFailTime = (int)time(0);
		break;
	}
}

void CClientNode::OnTaskDownloaded(int nTaskID, const string& sTaskZipFilePath)
{
	//������Ϣ��֪Master��
	_stDataPacket msgPacket;
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = C2L_CMD_TASKDOWN_SUCCESS;
	msgPacket.nClientID = m_pConfigure->nClientID;
	msgPacket.nOffset = nTaskID;
	msgPacket.nBufSize = (int)m_pConfigure->sAppType.length();
	assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
	strcpy(msgPacket.cBuffer, m_pConfigure->sAppType.c_str());
	Host2Net(msgPacket);
	if(network_send(m_nHeartSocket, (char *)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send C2L_CMD_TASKDOWN_SUCCESS message to MasterNode! %s:%d\n", __FILE__, __LINE__);
		m_nLastErrCode = MARC_CODE_SEND_FAILED;
	}

	//�����input��output�ļ�������
	CleanDir(m_pConfigure->sInputDir.c_str());
	CleanDir(m_pConfigure->sOutputDir.c_str());

	//��ѹ���ļ���ָ����Ŀ¼(input)
	char chCmd[1024] = {0};
#ifdef WIN32
	sprintf(chCmd,"%s unzip %s %s", MARC_MYZIP_APPCMD, sTaskZipFilePath.c_str(), m_pConfigure->sInputDir.c_str());
#else
	sprintf(chCmd, "/bin/tar -xzf %s -C %s ./", sTaskZipFilePath.c_str(), m_pConfigure->sInputDir.c_str());
#endif
	m_pLogger->Write(CRunLogger::LOG_INFO, "unzip task-zipfile, command: %s\n",chCmd);
	if(!Exec(chCmd))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "failed to unzip task-zipfile: %s, exit! %s:%d\n", chCmd, __FILE__, __LINE__);
		Sleep(1000);
		_exit(-1);
	}

	//��������ʽ�����ڵ����,xxx.exe input output ID
	m_nAppStartTime = (int)time(0);
	sprintf(chCmd, "%s %s %s %d", m_pConfigure->sAppCmd.c_str(), m_pConfigure->sInputDir.c_str(),m_pConfigure->sOutputDir.c_str(), m_pConfigure->nClientID);
	m_pLogger->Write(CRunLogger::LOG_INFO, "Startup Application program, command: %s\n", chCmd);
	if(m_pAppRunner->ExecuteApp(chCmd))
	{
		m_bAppStarted = true;
	}
	else
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to execute command: %s, exit! %s:%d\n", chCmd, __FILE__, __LINE__);
		Sleep(1000);
		_exit(-1);
	}

	//ɾ������ѹ���ļ�
	if(m_pConfigure->nAutoDeleteTaskFile != 0)
	{
		m_pLogger->Write(CRunLogger::LOG_INFO, "Auto delete task-zipfile: %s\n", sTaskZipFilePath.c_str());
		deleteFile(sTaskZipFilePath.c_str());
	}
}

void CClientNode::OnTaskDownloadFailed(int nTaskID, const string& sTaskZipFilePath)
{
	//������Ϣ��֪Master��
	_stDataPacket msgPacket;
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = C2L_CMD_TASKDOWN_FAILED;
	msgPacket.nClientID = m_pConfigure->nClientID;
	msgPacket.nOffset = nTaskID;
	msgPacket.nBufSize = (int)m_pConfigure->sAppType.length();
	assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
	strcpy(msgPacket.cBuffer, m_pConfigure->sAppType.c_str());
	Host2Net(msgPacket);
	if(network_send(m_nHeartSocket, (char *)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send C2L_CMD_TASKDOWN_FAILED message to MasterNode! %s:%d\n", __FILE__, __LINE__);
		m_nLastErrCode = MARC_CODE_SEND_FAILED;
	}

	//��¼״̬������ʧ�ܵĲ�����ִ��ʧ����������
	//m_stNodeStatus.nTotalFailedTasks++;
}

void CClientNode::KillApp()
{
	char chCmd[512] = {0};
	for(size_t i=0; i < m_pConfigure->oAppProcessList.size(); i++)
	{
		const char* sProName = m_pConfigure->oAppProcessList[i].sProName.c_str();
		const char* sProPath = m_pConfigure->oAppProcessList[i].sProPath.c_str();
		sprintf(chCmd, "%s %s %s", MARC_KILLPROLIST_SCRIPT_FILE, sProName, sProPath);
		m_pLogger->Write(CRunLogger::LOG_INFO, "Kill Application program '%s', command: %s\n", sProName, chCmd);
		if(!Exec(chCmd))
		{
			m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to execute command: %s, %s:%d\n", chCmd, __FILE__, __LINE__);
		}
	}
	m_pAppRunner->KillApp();
}

bool CClientNode::GetAppStateInfo(_stClientState &dAppState)
{
	//��־�ļ��Ƿ����
	string sClientStateFlagFile("./state/client.state.ok");
	NormalizePath(sClientStateFlagFile, false);
	if(!DIR_EXIST(sClientStateFlagFile.c_str())) return false;

	//���ļ�������ȡstate.nBufSize���ֽ�
	string sClientStateFile("./state/client.state");
	NormalizePath(sClientStateFile, false);
	if(!DIR_EXIST(sClientStateFile.c_str())) return false;
	dAppState.nBufSize = readFile(sClientStateFile.c_str(), 0, dAppState.cBuffer, sizeof(dAppState.cBuffer));

	//��ȡ���ɾ����־�ļ�
	deleteFile(sClientStateFlagFile.c_str());

	return true;
}

//��δ�ϴ����ļ���д����ʱ�ļ��Ա��´��������ϴ�
void CClientNode::SaveUploadResultFiles()
{
	if(m_pConfigure->nAutoSaveUploadFile == 0) return ;

	FILE *fp = fopen(MARC_REUPLOAD_RESULT_LISTFILE, "wb");
	if(fp != NULL)
	{
		LOCK(m_locker);
		while(!m_oUploadFiles.empty())
		{
			string sFileName = m_oUploadFiles.front();
			m_oUploadFiles.pop();
			fprintf(fp, "%s\n", sFileName.c_str());
			m_pLogger->Write(CRunLogger::LOG_INFO, "result-zipfile %s found\n", sFileName.c_str());
		}
		UNLOCK(m_locker);
		fclose(fp);
	}
	else
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't create file '%s'", MARC_REUPLOAD_RESULT_LISTFILE);
	}
}

void CClientNode::LoadUploadResultFile()
{
	if(m_pConfigure->nAutoSaveUploadFile == 0) return ;

	//��ȡmarc_reupload.result�м�¼�Ľ���ļ���oResultFiles����
	set<string> oResultFiles;
	FILE *fp = fopen(MARC_REUPLOAD_RESULT_LISTFILE, "rb");
	if(fp != NULL)
	{
		char sLine[1024];
		while (fgets(sLine, 1024, fp))
		{
			char sResultFilePath[512] = "";
			if(sscanf(sLine,"%s", sResultFilePath) != 1) continue;
			if (sResultFilePath[0] == 0) continue;
			
			if(!DIR_EXIST(sResultFilePath))
			{
				m_pLogger->Write(CRunLogger::LOG_WARNING, "result-zipfile '%s' not existed! %s:%d\n", sResultFilePath, __FILE__, __LINE__);
				continue;
			}
			oResultFiles.insert(sResultFilePath);
		}
		fclose(fp);
	}

	//��ȡResultĿ¼��δ�ϴ��Ľ���ļ���AutoDeleteResultFile��0ʱ����Ч��
	if(m_pConfigure->nAutoDeleteResultFile != 0)
	{
		CDirScanner ds(m_pConfigure->sZipResultDir.c_str(), false);
		const vector<string>& oZipFiles = ds.GetFileFullList();
		for(size_t i = 0; i < oZipFiles.size(); i++)
		{
			oResultFiles.insert(oZipFiles[i]);
		}
	}

	//��ն���
	LOCK(m_locker);
	while(!m_oUploadFiles.empty())
		m_oUploadFiles.pop();
	UNLOCK(m_locker);

	//������ϴ�����
	set<string>::const_iterator it = oResultFiles.begin();
	for(; it != oResultFiles.end(); ++it)
	{
		const string& sResultFile = (*it);
		m_pLogger->Write(CRunLogger::LOG_INFO, "result-zipfile found: %s, which will be uploaded later.\n", sResultFile.c_str());
		LOCK(m_locker);
		m_oUploadFiles.push(sResultFile);
		UNLOCK(m_locker);
	}
}

bool CClientNode::SendErrLogInfo(const char* sLog, void* arg)
{
	CClientNode* me = (CClientNode*)arg;
	if(sLog == NULL) return false;
	if(me->m_pConfigure->nUploadErrorLog == 0) return false;
	if(me->m_nHeartSocket == INVALID_SOCKET) return false;
	int nLogSize = strlen(sLog);
	if(nLogSize == 0) return false;

	_stDataPacket msgPacket;
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = C2L_CMD_ERRLOG_INFO;
	msgPacket.nClientID = me->m_pConfigure->nClientID;
	msgPacket.nOffset = 0;
	msgPacket.nBufSize = (nLogSize > MARC_MAX_MSG_BYTES ? MARC_MAX_MSG_BYTES : nLogSize);
	//assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
	strncpy(msgPacket.cBuffer, sLog, msgPacket.nBufSize);
	Host2Net(msgPacket);
	return network_send(me->m_nHeartSocket, (char *)&msgPacket, sizeof(_stDataPacket)) == MARC_NETWORK_OK;
}
