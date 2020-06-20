#include "ResultNode.h"
#include "../sftp/sftp_client.h"
#include "../utils/Utility.h"
#include "../utils/Network.h"
#include "../utils/LoopThread.h"
#include "../utils/RunLogger.h"
#include "../utils/myIniFile.h"
#include "../utils/DirScanner.h"


CResultNode::CResultNode(CResultConf* pConfigure, CRunLogger* pLogger)
{
	assert(pConfigure != NULL);
	m_pConfigure = pConfigure;
	m_pLogger = pLogger;
	if(m_pConfigure->nUploadErrorLog != 0)
		m_pLogger->SetErrLogCallback(SendErrLogInfo, this);
	m_pSftpSvr = NULL;
	m_sMasterIP = "";
	m_nMasterPort = 0;
	m_nHeartSocket = INVALID_SOCKET;
	m_nLastHeartTime = 0;
	m_nLastSourceStatusSendTime = 0;
	m_nLastAppVerCheckTime = 0;
	m_nLastErrCode = MARC_CODE_OK;
	m_bNeedRestart = false;

	m_pHeartThread = new CLoopThread();
	assert(m_pHeartThread != NULL);
	m_pHeartThread->SetRutine(HeartRutine, this);

	m_pResultThread = new CLoopThread();
	assert(m_pResultThread != NULL);
	m_pResultThread->SetRutine(ResultRutine, this);
	INITIALIZE_LOCKER(m_locker);

	memset(&m_stNodeStatus, 0, sizeof(_stResultStatus));
	m_stNodeStatus.nStartupTime = m_pConfigure->nStartupTime;
}

CResultNode::~CResultNode()
{
	m_pLogger->SetErrLogCallback(NULL, NULL);
	if(m_pHeartThread != NULL)
		delete m_pHeartThread;
	if(m_pResultThread != NULL)
		delete m_pResultThread;
	DESTROY_LOCKER(m_locker);
}

bool CResultNode::Start(bool bUseBakMaster)
{
	m_bNeedRestart = false;
	m_nLastErrCode = MARC_CODE_OK;

	//Master��IP�Ͷ˿�
	m_sMasterIP = bUseBakMaster?m_pConfigure->sBakMasterIP:m_pConfigure->sMasterIP;
	m_nMasterPort = bUseBakMaster?m_pConfigure->nBakMasterPort:m_pConfigure->nMasterPort;

	//������ǰδ������Ľ��ѹ���ļ�
	m_pLogger->Write(CRunLogger::LOG_INFO, "Load unfinished result-zipfile cached before...\n");
	LoadUnfinishedResultZipFile();

	//��ʼ��m_pSftpSvr
	m_pSftpSvr = sftp_server_init(m_pConfigure->sListenIP.c_str(), m_pConfigure->nListenPort, MARC_MAX_CONN_COUNT);
	if(m_pSftpSvr == NULL)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "sftp_server_init failed, please check IP(%s) and port(%d) %s:%d\n",
			m_pConfigure->sListenIP.c_str(), m_pConfigure->nListenPort, __FILE__, __LINE__);
		m_bNeedRestart = true;
		return false;
	}
	sftp_server_setopt(m_pSftpSvr, SFTPOPT_MAX_DATA_PACKET_SIZE, m_pConfigure->nMaxPacketSize);
	sftp_server_setopt(m_pSftpSvr, SFTPOPT_UPLOAD_FINISHED_FUNCTION, OnResultUploaded);
	sftp_server_setopt(m_pSftpSvr, SFTPOPT_PRIVATE_DATA, this);
	sftp_server_setopt(m_pSftpSvr, SFTPOPT_CONNECTION_TIMEOUT, m_pConfigure->nUploadTimeout);
	if(!sftp_server_start(m_pSftpSvr))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "sftp_server_start failed. %s:%d\n", __FILE__, __LINE__);
		sftp_server_exit(m_pSftpSvr);
		m_pSftpSvr = NULL;
		m_bNeedRestart = true;
		return false;
	}

	//ע�ᵽmaster��	
	int nRetCode = RegisterToMaster();
	if(nRetCode != MARC_CODE_OK) 
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to register to MasterNode!\n");
		sftp_server_stop(m_pSftpSvr);
		sftp_server_exit(m_pSftpSvr);
		m_pSftpSvr = NULL;
		m_nLastErrCode = nRetCode;
		m_bNeedRestart = true;
		return false;
	}
	m_pLogger->Write(CRunLogger::LOG_INFO, "Register to MasterNode successfully, assigned ID=%d\n", m_pConfigure->nResultID);

	//���������߳�
	assert(m_pHeartThread != NULL);
	if(!m_pHeartThread->Start())
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to start thread! %s:%d\n", __FILE__, __LINE__);
		m_bNeedRestart = true;
		return false;
	}

	//������������߳�
	assert(m_pResultThread != NULL);
	if(!m_pResultThread->Start())
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to start thread! %s:%d\n", __FILE__, __LINE__);
		m_bNeedRestart = true;
		return false;
	}	

	return true;
}

void CResultNode::Stop()
{
	//ֹͣ��������߳�
	assert(m_pResultThread != NULL);
	m_pResultThread->Stop();

	//ֹͣ�����߳�
	assert(m_pHeartThread != NULL);
	m_pHeartThread->Stop();

	//ȡ��ע��
	UnregisterToMaster();

	//ֹͣm_pSftpSvr
	if(m_pSftpSvr != NULL)
	{
		sftp_server_stop(m_pSftpSvr);
		sftp_server_exit(m_pSftpSvr);
		m_pSftpSvr = NULL;
	}

	//����δ����Ľ���ļ�
	m_pLogger->Write(CRunLogger::LOG_INFO, "Save unfinished result-zipfile...\n");
	SaveUnfinishedResultZipFile();	
}

int CResultNode::RegisterToMaster()
{
	m_pLogger->Write(CRunLogger::LOG_INFO, "Register to MasterNode(%s:%d)...\n", m_sMasterIP.c_str(), m_nMasterPort);

	//����Master
	m_nHeartSocket = network_connect(m_sMasterIP.c_str(), m_nMasterPort);
	if(m_nHeartSocket == INVALID_SOCKET) 
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't connect to MasterNode(%s:%d)! %s:%d\n", m_sMasterIP.c_str(), m_nMasterPort, __FILE__, __LINE__);
		return MARC_CODE_CONNECT_FAILED;
	}

	//���ݸ�Master����Ϣ
	_stResultNode rstNode;
	memset(&rstNode, 0, sizeof(_stResultNode));
	if(m_pConfigure->nNatEnabled == 0)
		strcpy(rstNode.chIp, m_pConfigure->sListenIP.c_str());
	else
		strcpy(rstNode.chIp, m_pConfigure->sNatIP.c_str());
	rstNode.iPort = m_pConfigure->nListenPort;
	assert(m_pConfigure->sAppType.length() < sizeof(rstNode.chAppType));
	strcpy(rstNode.chAppType, m_pConfigure->sAppType.c_str());
	strcpy(rstNode.chSavePath, m_pConfigure->sZipResultDir.c_str());
	Host2Net(rstNode);

	//����ע��������Ϣ
	_stDataPacket dataPacket;
	memset(&dataPacket, 0, sizeof(_stDataPacket));
	dataPacket.nCommand = R2M_CMD_REGISTER_REQ;
	dataPacket.nOffset = MARC_MAGIC_NUMBER;
	dataPacket.nClientID = m_pConfigure->nResultID;
	memcpy(dataPacket.cBuffer, (char *)&rstNode, sizeof(_stResultNode));
	Host2Net(dataPacket);
	if(network_send(m_nHeartSocket, (char *)&dataPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		CLOSE_SOCKET(m_nHeartSocket);
		m_nHeartSocket = INVALID_SOCKET;
		m_pLogger->Write(CRunLogger::LOG_ERROR, "network_send failed! %s:%d\n", __FILE__, __LINE__);
		return MARC_CODE_SEND_FAILED;
	}

	//����Master���ص�Ӧ����Ϣ
	memset(&dataPacket, 0, sizeof(_stDataPacket));
	if(network_recv(m_nHeartSocket, (char*)&dataPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		CLOSE_SOCKET(m_nHeartSocket);
		m_nHeartSocket = INVALID_SOCKET;
		m_pLogger->Write(CRunLogger::LOG_ERROR, "network_recv failed! %s:%d\n", __FILE__, __LINE__);
		return MARC_CODE_RECV_FAILED;
	}
	Net2Host(dataPacket);
	switch(dataPacket.nCommand)
	{
	case M2R_CMD_REGISTER_YES: //ע��ɹ�
		assert(dataPacket.nClientID > 0);
		m_pConfigure->nResultID = dataPacket.nClientID;
		SendInstallPath(); //������·����֪Master�ڵ�
		return MARC_CODE_OK;
	case M2R_CMD_REGISTER_NO: //ע��ʧ��
		CLOSE_SOCKET(m_nHeartSocket);
		m_nHeartSocket = INVALID_SOCKET;
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Register to MasterNode refused! %s:%d\n", __FILE__, __LINE__);
		return MARC_CODE_REGISTER_REFUSED;
	default: //��Ч����
		CLOSE_SOCKET(m_nHeartSocket);
		m_nHeartSocket = INVALID_SOCKET;
		return MARC_CODE_INVALID_COMMAND;
	};
}

void CResultNode::UnregisterToMaster()
{
	//�Ͽ���Master�˵�����
	if(m_nHeartSocket != INVALID_SOCKET)
	{
		_stDataPacket msgPacket;
		memset(&msgPacket, 0, sizeof(_stDataPacket));
		msgPacket.nCommand = R2M_CMD_UNREGISTER;
		msgPacket.nClientID = m_pConfigure->nResultID;
		msgPacket.nBufSize = (int)m_pConfigure->sAppType.length();
		assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
		strcpy(msgPacket.cBuffer, m_pConfigure->sAppType.c_str());
		Host2Net(msgPacket);
		network_send(m_nHeartSocket, (char*)&msgPacket, sizeof(_stDataPacket));
		CLOSE_SOCKET(m_nHeartSocket);
		m_nHeartSocket = INVALID_SOCKET;
	}
}

void CResultNode::HandleMasterMsg(const _stDataPacket &msgPacket)
{
	//ĿǰMaster�ڵ㲻��ͨ���������Ӹ�Result�ڵ㷢��Ϣ
	m_pLogger->Write(CRunLogger::LOG_INFO, "Receive message(command:%d) from MasterNode.\n", msgPacket.nCommand);
}

void CResultNode::SendInstallPath()
{
	if(m_nHeartSocket != INVALID_SOCKET)
	{
		_stDataPacket msgPacket;
		memset(&msgPacket, 0, sizeof(_stDataPacket));
		msgPacket.nCommand = R2M_CMD_INSTALL_PATH;
		msgPacket.nClientID = m_pConfigure->nResultID;
		msgPacket.nBufSize = (int)m_pConfigure->sInstallPath.length();
		assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
		strcpy(msgPacket.cBuffer, m_pConfigure->sInstallPath.c_str());
		Host2Net(msgPacket);
		if(network_send(m_nHeartSocket, (char*)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
		{
			CLOSE_SOCKET(m_nHeartSocket);
			m_nHeartSocket = INVALID_SOCKET;
			m_pLogger->Write(CRunLogger::LOG_ERROR, "network_send failed! %s:%d\n", __FILE__, __LINE__);
		}
	}
}


void CResultNode::HeartRutine(void *param)
{
	CResultNode *me = (CResultNode *)param;
	if(me->m_nHeartSocket == INVALID_SOCKET) return ;

	//���½ڵ�״̬֮����
	LOCK(me->m_locker);
	unsigned int nOverload = (unsigned int)me->m_oResultZipFiles.size();
	me->m_stNodeStatus.nOverload = nOverload;
	UNLOCK(me->m_locker);

	//fdset��ʼ��
	fd_set fdRead, fdWrite, fdException;
	FD_ZERO(&fdRead);
	FD_ZERO(&fdWrite);
	FD_ZERO(&fdException);
	FD_SET(me->m_nHeartSocket, &fdRead);
	FD_SET(me->m_nHeartSocket, &fdWrite);
	FD_SET(me->m_nHeartSocket, &fdException);

	//select����
	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;
	int inReady = select((int)me->m_nHeartSocket+1, &fdRead, &fdWrite, &fdException, &timeout);
	if(inReady == 0) return ; //��ʱ
	if(inReady < 0)  //����
	{
		CLOSE_SOCKET(me->m_nHeartSocket);
		me->m_nHeartSocket = INVALID_SOCKET;
		me->m_nLastErrCode = MARC_CODE_SELECT_FAILED;
		me->m_pLogger->Write(CRunLogger::LOG_ERROR, "select error: %s! %s:%d\n", strerror(errno), __FILE__, __LINE__);
		me->m_bNeedRestart = true;
		Sleep(2000);
		return ; 
	}
	if(FD_ISSET(me->m_nHeartSocket, &fdException)) //�쳣
	{
		CLOSE_SOCKET(me->m_nHeartSocket);
		me->m_nHeartSocket = INVALID_SOCKET;
		me->m_nLastErrCode = MARC_CODE_SOCKET_EXCEPTION;
		me->m_pLogger->Write(CRunLogger::LOG_ERROR, "socket exception! %s:%d\n", __FILE__, __LINE__);
		me->m_bNeedRestart = true;
		Sleep(2000);
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
		case MARC_NETWORK_CLOSED: //���ӱ�Master�ڵ�ر�
			CLOSE_SOCKET(me->m_nHeartSocket);
			me->m_nHeartSocket = INVALID_SOCKET;
			me->m_nLastErrCode = MARC_CODE_SOCKET_EXCEPTION;
			me->m_pLogger->Write(CRunLogger::LOG_ERROR, "Connection to MasterNode closed, maybe MasterNode exited! %s:%d\n", __FILE__, __LINE__);
			me->m_bNeedRestart = true;
			return ;
		case MARC_NETWORK_ERROR: //�������
			CLOSE_SOCKET(me->m_nHeartSocket);
			me->m_nHeartSocket = INVALID_SOCKET;
			me->m_nLastErrCode = MARC_CODE_SOCKET_EXCEPTION;
			me->m_pLogger->Write(CRunLogger::LOG_ERROR, "Network error: %s! %s:%d\n", strerror(errno), __FILE__, __LINE__);
			me->m_bNeedRestart = true;
			return ;
		default:
			assert(false); //Ӧ�ò��ᵽ��
			break ;
		};
	}
	if(FD_ISSET(me->m_nHeartSocket, &fdWrite))
	{
		//����������Ϣ
		if(time(0) - me->m_nLastHeartTime >= me->m_pConfigure->nHeartbeatInterval)
		{
			me->m_nLastHeartTime = (int)time(0);
			
			//ȡ�õ�ǰ�ڵ�״̬
			LOCK(me->m_locker);
			_stResultStatus status;
			memcpy(&status, &me->m_stNodeStatus, sizeof(_stResultStatus));
			Host2Net(status);
			UNLOCK(me->m_locker);

			//����������Ϣ��ͬʱ����ǰ�ڵ��״̬��Ϣ����Master��
			_stDataPacket msgPacket;
			memset(&msgPacket, 0, sizeof(_stDataPacket));
			msgPacket.nCommand = R2M_CMD_HEART_SEND;
			msgPacket.nClientID = me->m_pConfigure->nResultID;
			msgPacket.nOffset = 0; 
			msgPacket.nBufSize = sizeof(_stResultStatus);
			assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
			memcpy(msgPacket.cBuffer, (char *)&status, sizeof(_stResultStatus));
			Host2Net(msgPacket);
			int iRet = network_send(me->m_nHeartSocket, (char *)&msgPacket, sizeof(_stDataPacket));
			switch(iRet)
			{
			case MARC_NETWORK_OK: //���ͳɹ�
				me->m_pLogger->Write(CRunLogger::LOG_INFO, "Send heartbeat to MasterNode successfully, overload=%d\n", nOverload);
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
				me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Can't accquire source status!\n");
			}
			else
			{
				//���͸�Master
				_stDataPacket msgPacket;
				memset(&msgPacket, 0, sizeof(_stDataPacket));
				msgPacket.nCommand = R2M_CMD_SOURCE_STATUS;
				msgPacket.nClientID = me->m_pConfigure->nResultID;
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
					me->m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send source status to MasterNode! %s:%d\n", __FILE__, __LINE__);
					//me->m_bNeedRestart = true;
					break;
				}
			}
		}
#endif //WIN32
	}

	Sleep(1000);
}

void CResultNode::OnResultUploaded(CResultNode* me, const char* sResultZipFilePath)
{
	me->m_pLogger->Write(CRunLogger::LOG_INFO, "result-zipfile received from ClientNode successfully, add to queue: %s\n", sResultZipFilePath);
	LOCK(me->m_locker);
	me->m_oResultZipFiles.push(make_pair(sResultZipFilePath,0));
	me->m_stNodeStatus.nTotalReceived++;
	me->m_stNodeStatus.nLastReceivedTime = time(0);
	UNLOCK(me->m_locker);
}

void CResultNode::ResultRutine(void *param)
{
	CResultNode *me = (CResultNode *)param;
	pair<string,int> oZipFilePath;
	string sZipFilePath = "";
	int nFailCount = 0;
	LOCK(me->m_locker);
	if(!me->m_oResultZipFiles.empty())
	{
		oZipFilePath = me->m_oResultZipFiles.front();
		me->m_oResultZipFiles.pop();
		sZipFilePath = oZipFilePath.first;
		nFailCount = oZipFilePath.second;
	}
	UNLOCK(me->m_locker);

	if(!sZipFilePath.empty())
	{
		me->m_pLogger->Write(CRunLogger::LOG_INFO, "Begin to process result-zipfile: %s\n", sZipFilePath.c_str());
		if(!DIR_EXIST(sZipFilePath.c_str()))
		{
			me->m_pLogger->Write(CRunLogger::LOG_WARNING, "result-zipfile not existed: %s %s:%d\n", 
				sZipFilePath.c_str(), __FILE__, __LINE__);
			LOCK(me->m_locker);
			me->m_stNodeStatus.nTotalFailed++;
			UNLOCK(me->m_locker);
			return ;
		}
		if(nFailCount > me->m_pConfigure->nResultFailMaxRetry)
		{
			me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Failure count of result-zipfile '%s' exceeded maximum, ignore it! %s:%d\n", 
				sZipFilePath.c_str(), __FILE__, __LINE__);
			deleteFile(sZipFilePath.c_str());
			LOCK(me->m_locker);
			me->m_stNodeStatus.nTotalAbandoned++;
			UNLOCK(me->m_locker);
			return ;
		}

		time_t nTimeUsed = 0;
		if(!me->ProcessResultZipFile(sZipFilePath, nTimeUsed))
		{
			me->m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to process result-zipfile '%s', which will be processed later! %s:%d\n", 
				sZipFilePath.c_str(), __FILE__, __LINE__);
			LOCK(me->m_locker);
			me->m_oResultZipFiles.push(make_pair(sZipFilePath, nFailCount+1));
			me->m_stNodeStatus.nTotalFailed++;
			UNLOCK(me->m_locker);
		}
		else
		{
			LOCK(me->m_locker);
			me->m_stNodeStatus.nTotalFinished++;
			me->m_stNodeStatus.nTotalTimeUsed += nTimeUsed;
			UNLOCK(me->m_locker);
			me->m_pLogger->Write(CRunLogger::LOG_INFO, "result-zipfile processed successfully: %s\n", sZipFilePath.c_str());
			if(me->m_pConfigure->nAutoDeleteResultFile != 0)
			{
				me->m_pLogger->Write(CRunLogger::LOG_INFO, "Auto delete result-zipfile: %s\n", sZipFilePath.c_str());
				deleteFile(sZipFilePath.c_str());
			}
		}
	}
}

bool CResultNode::ProcessResultZipFile(const string& sResultZipFilePath, time_t &nTimeUsed)
{
	time_t t1 = time(0);

	//�����ļ������ClientID�ͽ������(1_NewsGather_result_20100204095923 ->1, NewsGather)
	string sFileName = getFileName(sResultZipFilePath, false);
	string::size_type pos1 = sFileName.find('_');
	if(pos1 == string::npos) 
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Illegal result-zipfile name: %s, %s:%d\n", sResultZipFilePath.c_str(), __FILE__, __LINE__);
		return false;
	}
	int nClientID = atoi(string(sFileName, 0, pos1).c_str());
	pos1++;
	string::size_type pos2 = sFileName.find('_', pos1);
	if(pos2 == string::npos) 
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Illegal result-zipfile name: %s, %s:%d\n", sResultZipFilePath.c_str(), __FILE__, __LINE__);
		return false;
	}
	string sAppType = string(sFileName, pos1, pos2 - pos1);

	//�״δ����AppType�����������汾��Ϊ0
	if(m_pConfigure->oCurAppVersion.find(sAppType) == m_pConfigure->oCurAppVersion.end())
		m_pConfigure->oCurAppVersion[sAppType] = 0;

	//���ļ���ѹ����ӦĿ¼(��./data/1_NewsGather_result_20100204095923/)
	char chResultPath[1024] = {0};
	sprintf(chResultPath, "%s%s/", m_pConfigure->sDataDir.c_str(), sFileName.c_str());
	char chCmd[1024] = {0};
#ifdef WIN32
	sprintf(chCmd, "%s unzip %s %s", MARC_MYZIP_APPCMD, sResultZipFilePath.c_str(), chResultPath);
#else
	sprintf(chCmd, "/bin/tar -xzf %s -C %s ./", sResultZipFilePath.c_str(), chResultPath);
#endif
	NormalizePath(chCmd, false);
	if(!CreateFilePath(chResultPath))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "can't create file path: %s\n", chResultPath);
		return false;
	}
	m_pLogger->Write(CRunLogger::LOG_INFO, "unzip result-zipfile, command: %s\n", chCmd);
	if(!Exec(chCmd))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "unzip failed: %s! %s:%d\n", chCmd, __FILE__, __LINE__);
		return false;
	}

	//���App�汾�����и�������а汾����
	if(m_pConfigure->nUpdateEnabled != 0 && time(0) - m_nLastAppVerCheckTime > m_pConfigure->nUpdateInterval)
	{
		m_nLastAppVerCheckTime = time(0);
		_stAppVerInfo dAppVerInfo;
		if(CheckAppVersion(sAppType, dAppVerInfo))
			UpdateAppVersion(sAppType, dAppVerInfo);
	}

	//��ý����������
	string sAppCmd = "";
	if(!GetAppCmd(sAppType, sAppCmd)) return false;

	//����ִ�н���������, ִ�нӿڣ�[AppCmd] [ResultPath] [ClientID]
	//��./NewsStore ./data/1_NewsGather_result_20100204095923/ 1
	sprintf(chCmd,"%s %s %d", sAppCmd.c_str(), chResultPath, nClientID);
	m_pLogger->Write(CRunLogger::LOG_INFO, "Begin to execute application program for the result, command: %s\n", chCmd);
	NormalizePath(chCmd, false);
	if(!Exec(chCmd, m_pConfigure->nAppRunTimeout))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to execute command: %s, %s:%d\n", chCmd, __FILE__, __LINE__);
		deleteDir(chResultPath);
		return false;
	}

	//�������������Ƿ�ִ�гɹ������ɹ�����[ResultPath]������.success�ļ���
	string sFlagFile = string(chResultPath) + ".success";
	if(DIR_EXIST(sFlagFile.c_str()))
	{
		m_pLogger->Write(CRunLogger::LOG_INFO, "Execute application program for the result successfully, delete result-folder: %s\n", chResultPath);
		deleteDir(chResultPath);
		nTimeUsed = time(0) - t1;
		return true;
	}
	else
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "Failed to execute application program for the result(flag file '%s' not found)! %s:%d\n", sFlagFile.c_str(), __FILE__, __LINE__);
		m_pLogger->Write(CRunLogger::LOG_INFO, "Delete result-folder: %s\n", chResultPath);
		deleteDir(chResultPath);
		return false;
	}
}

bool CResultNode::GetAppCmd(const string& sAppType, string& sAppCmd)
{
	//�Ȳ�ѯ�ڴ����Ƿ�����
	map<string,string>::const_iterator it = m_pConfigure->oAppType2AppCmd.find(sAppType);
	if(it != m_pConfigure->oAppType2AppCmd.end())
	{
		sAppCmd = it->second;
		return true;
	}

	//�������ļ��ж�ȡ
	if(!INI::CMyIniFile::ReadIniStr(MARC_RESULT_CONF_FILE, "appcmd", sAppType.c_str(), sAppCmd) || sAppCmd.empty())
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't find application command for '%s' in configue file '%s'! %s:%d\n", 
			sAppType.c_str(), MARC_RESULT_CONF_FILE, __FILE__, __LINE__);
		return false;
	}
	NormalizePath(sAppCmd, false);
	m_pConfigure->oAppType2AppCmd[sAppType] = sAppCmd;
	return true;
}

void CResultNode::SaveUnfinishedResultZipFile()
{	
	if(m_pConfigure->nAutoSaveUnfinishedResultFile == 0) return ;

	FILE *fp = fopen(MARC_UNFINISHED_RESULT_LISTFILE, "wb");
	if(fp != NULL)
	{
		LOCK(m_locker);
		while(!m_oResultZipFiles.empty())
		{
			pair<string,int> oZipFilePath = m_oResultZipFiles.front();
			m_oResultZipFiles.pop();
			const string& sResultFile = oZipFilePath.first;
			fprintf(fp, "%s\n", sResultFile.c_str());
			m_pLogger->Write(CRunLogger::LOG_INFO, "result-zipfile '%s' found\n", sResultFile.c_str());
		}
		UNLOCK(m_locker);
		fclose(fp);
	}
	else
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't create the file: %s", MARC_UNFINISHED_RESULT_LISTFILE);
	}
}

//�����ϴ������˳�ʱδ����Ľ��ѹ���ļ�
void CResultNode::LoadUnfinishedResultZipFile()
{
	if(m_pConfigure->nAutoSaveUnfinishedResultFile == 0) return ;

	//��ȡmarc_unfinished.result�м�¼�Ľ���ļ���oResultFiles����
	set<string> oResultFiles;
	FILE *fp = fopen(MARC_UNFINISHED_RESULT_LISTFILE, "rb");
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
				m_pLogger->Write(CRunLogger::LOG_WARNING, "File '%s' not existed! %s:%d\n", sResultFilePath, __FILE__, __LINE__);
				continue;
			}
			oResultFiles.insert(sResultFilePath);
		}
		fclose(fp);
	}

	//��ȡ��ǰδ����Ľ���ļ�(����AutoDeleteResultFile��0������)
	if(m_pConfigure->nAutoDeleteResultFile != 0)
	{
		CDirScanner ds(m_pConfigure->sZipResultDir.c_str(), false);
		const vector<string>& oZipFiles = ds.GetFileFullList();
		for(size_t i = 0; i < oZipFiles.size(); i++)
		{
			oResultFiles.insert(oZipFiles[i]);
		}
	}

	//��մ��������
	LOCK(m_locker);
	while(!m_oResultZipFiles.empty())
		m_oResultZipFiles.pop();
	UNLOCK(m_locker);

	//������������
	set<string>::const_iterator it = oResultFiles.begin();
	for(; it != oResultFiles.end(); ++it)
	{
		const string& sResultFile = (*it);
		m_pLogger->Write(CRunLogger::LOG_INFO, "result-zip file found: %s\n", sResultFile.c_str());
		LOCK(m_locker);
		m_oResultZipFiles.push(make_pair(sResultFile, 0));
		UNLOCK(m_locker);
	}
}

bool CResultNode::SendErrLogInfo(const char* sLog, void* arg)
{
	CResultNode* me = (CResultNode*)arg;
	if(sLog == NULL) return false;
	if(me->m_pConfigure->nUploadErrorLog == 0) return false;
	if(me->m_nHeartSocket == INVALID_SOCKET) return false;
	int nLogSize = strlen(sLog);
	if(nLogSize == 0) return false;

	_stDataPacket msgPacket;
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = R2M_CMD_ERRLOG_INFO;
	msgPacket.nClientID = me->m_pConfigure->nResultID;
	msgPacket.nOffset = 0;
	msgPacket.nBufSize = (nLogSize > MARC_MAX_MSG_BYTES ? MARC_MAX_MSG_BYTES : nLogSize);
	assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
	strncpy(msgPacket.cBuffer, sLog, msgPacket.nBufSize);
	Host2Net(msgPacket);
	return network_send(me->m_nHeartSocket, (char *)&msgPacket, sizeof(_stDataPacket)) == MARC_NETWORK_OK;
}

bool CResultNode::CheckAppVersion(const string& sAppType, _stAppVerInfo &dAppVerInfo)
{
	//���ӵ�Master�ڵ��������
	m_pLogger->Write(CRunLogger::LOG_INFO, "Request to MasterNode for application version update...\n");
	SOCKET nSocket = network_connect(m_sMasterIP.c_str(), m_nMasterPort);
	if(nSocket == INVALID_SOCKET) 
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't connect to MasterNode! %s:%d\n", __FILE__, __LINE__);
		return false;
	}

	//���sAppType�ĵ�ǰ�汾��
	int nCurAppVersion = 0;
	map<string,int>::const_iterator it = m_pConfigure->oCurAppVersion.find(sAppType);
	if(it != m_pConfigure->oCurAppVersion.end())
		nCurAppVersion = it->second;

	//��Master�ڵ㷢��APP�汾��������
	_stDataPacket msgPacket;
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = R2M_CMD_APPVER_REQ;
	msgPacket.nClientID = m_pConfigure->nResultID;
	msgPacket.nOffset = nCurAppVersion;
	msgPacket.nBufSize = (int)sAppType.length();
	assert(msgPacket.nBufSize <= MARC_MAX_MSG_BYTES);
	strcpy(msgPacket.cBuffer, sAppType.c_str());
	Host2Net(msgPacket);
	if(network_send(nSocket, (char*)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK) 
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "network_send R2M_CMD_APPVER_REQ failed! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return false;
	}
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	if(network_recv(nSocket, (char*)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "network_recv of R2M_CMD_APPVER_REQ failed! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return false;
	}
	Net2Host(msgPacket);

	//�����Ӧ��Ϣ
	bool bNeedUpdate = false;
	switch(msgPacket.nCommand)
	{
	case M2R_CMD_APPVER_YES:
		memcpy(&dAppVerInfo, msgPacket.cBuffer, sizeof(_stAppVerInfo));
		Net2Host(dAppVerInfo);
		bNeedUpdate = true;
		m_pLogger->Write(CRunLogger::LOG_INFO, "Application needs to be updated, The newest version is %d\n", dAppVerInfo.nUpdateVersion);
		break;
	case M2R_CMD_APPVER_NO:
		m_pLogger->Write(CRunLogger::LOG_INFO, "Application doesn't need to be updated\n");
		break;
	default:
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid message command: %d\n", msgPacket.nCommand);
		break;
	};

	//�ر���Master�ڵ�������������
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	msgPacket.nCommand = R2M_CMD_CLOSE;
	msgPacket.nClientID = m_pConfigure->nResultID;
	Host2Net(msgPacket);
	network_send(nSocket, (char *)&msgPacket, sizeof(_stDataPacket));
	//Sleep(1000);
	CLOSE_SOCKET(nSocket);

	return bNeedUpdate;
}

void CResultNode::UpdateAppVersion(const string& sAppType, const _stAppVerInfo &dAppVerInfo)
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
		m_pLogger->Write(CRunLogger::LOG_INFO, "update-zipfile downloaded successfully (version=%d, file=%s), %d seconds",
			dAppVerInfo.nUpdateVersion, sLocalZipFile.c_str(), nTimeUsed);
		break;
	default: //��������
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to download update-zipfile(version=%d, File=%s), ErrCode: %d, %s:%d",
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
	m_pConfigure->oCurAppVersion[sAppType] = dAppVerInfo.nUpdateVersion;
	string sAppVerFile = sAppType + ".marc.ver";
	WriteAppVersion(sAppVerFile, sAppType, dAppVerInfo.nUpdateVersion);
	m_pLogger->Write(CRunLogger::LOG_INFO, "Application(AppType=%s) updated successfully! New version is %d\n", sAppType.c_str(), dAppVerInfo.nUpdateVersion);
}

