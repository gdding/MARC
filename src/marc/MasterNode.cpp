#include "MasterNode.h"
#include "ResultNodeManager.h"
#include "TaskManager.h"
#include "MasterListener.h"
#include "ClientManager.h"
#include "../utils/Utility.h"
#include "../utils/Network.h"
#include "../utils/LoopThread.h"
#include "../utils/DirScanner.h"
#include "../utils/RunLogger.h"
#include "../utils/myIniFile.h"


int CMasterNode::m_nClientIDBase = 10001;
int CMasterNode::m_nResultIDBase = 60001;
CMasterNode::CMasterNode(CMasterConf* pConfigure, CRunLogger* pLogger)
{
	m_pConfigure = pConfigure;
	assert(pLogger != NULL);
	m_pLogger = pLogger;

	m_nListenSocket = INVALID_SOCKET;
	m_nMaxSocket = INVALID_SOCKET;
	m_pTaskSvr = NULL;
	memset(&m_dSourceStatus, 0, sizeof(_stNodeSourceStatus));
	INITIALIZE_LOCKER(m_locker);

	m_pTaskStatInfo = new CTaskStatInfo();
	assert(m_pTaskStatInfo != NULL);

	m_pTaskManager = new CTaskManager(pConfigure, m_pTaskStatInfo, pLogger);
	assert(m_pTaskManager != NULL);

	m_pResultNodeManager = new CResultNodeManager(pConfigure, pLogger);
	assert(m_pResultNodeManager != NULL);

	m_pClientManager = new CClientManager(m_pTaskManager, pConfigure, pLogger);
	assert(m_pClientManager != NULL);

	m_pMasterThread = new CLoopThread();
	assert(m_pMasterThread != NULL);
	m_pMasterThread->SetRutine(MasterRutine, this);

	m_pTaskThread = new CLoopThread();
	assert(m_pTaskThread != NULL);
	m_pTaskThread->SetRutine(TaskRutine, this);

	m_pStateSaveThread = new CLoopThread();
	assert(m_pStateSaveThread != NULL);
	m_pStateSaveThread->SetRutine(StateSaveRutine, this);
	m_nLastSaveTime = 0;

	m_pWatchThread = new CLoopThread();
	assert(m_pWatchThread != NULL);
	m_pWatchThread->SetRutine(WatchRutine, this);
	m_nLastWatchTime = 0;
}

CMasterNode::~CMasterNode()
{
	if(m_pClientManager != NULL) 
		delete m_pClientManager;
	if(m_pTaskManager != NULL)
		delete m_pTaskManager;
	if(m_pResultNodeManager != NULL)
		delete m_pResultNodeManager;
	if(m_pTaskStatInfo != NULL)
		delete m_pTaskStatInfo;
	if(m_pWatchThread != NULL)
		delete m_pWatchThread;
	if(m_pMasterThread != NULL)
		delete m_pMasterThread;
	if(m_pTaskThread != NULL)
		delete m_pTaskThread;
	if(m_pStateSaveThread != NULL)
		delete m_pStateSaveThread;
	DESTROY_LOCKER(m_locker);
}

bool CMasterNode::Start()
{
	//��ʼ��Master����socket
	m_nListenSocket = network_listener_init(m_pConfigure->sIP.c_str(), m_pConfigure->nPort, MARC_MAX_CONN_COUNT);
	if(m_nListenSocket == INVALID_SOCKET)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "MasterNode init failed! Please check IP(%s) and Port(%d)! %s:%d\n",
			m_pConfigure->sIP.c_str(), m_pConfigure->nPort, __FILE__, __LINE__);
		return false;
	}
	m_nMaxSocket = m_nListenSocket;

	//fdset��ʼ��
	FD_ZERO(&m_fdAllSet);
	FD_SET(m_nListenSocket, &m_fdAllSet);

	//��ʼ���������ط�������֮
	//m_pLogger->Write(CRunLogger::LOG_INFO, "Initialize task download server and startup...\n", m_pConfigure->nMaxListenerCount);
	m_sTaskSvrIP = m_pConfigure->sIP;
	m_nTaskSvrPort = m_pConfigure->nPort+1;
	while(true)
	{
		m_pTaskSvr = sftp_server_init(m_sTaskSvrIP.c_str(), m_nTaskSvrPort, MARC_MAX_CONN_COUNT);
		if(m_pTaskSvr != NULL)
		{
			//m_pLogger->Write(CRunLogger::LOG_INFO, "Task download server startup successfully, listening on %s:%d\n", m_sTaskSvrIP.c_str(), m_nTaskSvrPort);
			m_pConfigure->nTaskPort = m_nTaskSvrPort;
			break;
		}

		//ʧ����һ���˿�
		m_pLogger->Write(CRunLogger::LOG_WARNING, "Task download server startup failed, change port.\n", m_sTaskSvrIP.c_str(), m_nTaskSvrPort);
		m_nTaskSvrPort++;
	}
	sftp_server_setopt(m_pTaskSvr, SFTPOPT_MAX_DATA_PACKET_SIZE, m_pConfigure->nMaxPacketSize);
	sftp_server_setopt(m_pTaskSvr, SFTPOPT_DOWNLOAD_FINISHED_FUNCTION, OnTaskDownloaded);
	sftp_server_setopt(m_pTaskSvr, SFTPOPT_PRIVATE_DATA, this);
	sftp_server_setopt(m_pTaskSvr, SFTPOPT_CONNECTION_TIMEOUT, m_pConfigure->nMaxTaskFetchTime);
	if(!sftp_server_start(m_pTaskSvr))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to startup task download server! %s:%d\n", __FILE__, __LINE__);
		sftp_server_exit(m_pTaskSvr);
		m_pTaskSvr = NULL;
		return false;
	}

	//��ʼ�����������鲢����֮
	m_pLogger->Write(CRunLogger::LOG_INFO, "Initialize listener group(totally %d listeners)...\n", m_pConfigure->nMaxListenerCount);
	for(int i=0; i<m_pConfigure->nMaxListenerCount; i++)
	{
		CMasterListener *pListener = new CMasterListener(m_pConfigure, m_pTaskManager, m_pResultNodeManager, m_pClientManager, m_pLogger);
		assert(pListener != NULL);
		if(!pListener->Start(m_pConfigure->sIP.c_str(), m_nTaskSvrPort, MARC_MAX_CONN_COUNT))
		{
			m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to startup listener! %s:%d\n", __FILE__, __LINE__);
			delete pListener;
			sftp_server_stop(m_pTaskSvr);
			sftp_server_exit(m_pTaskSvr);
			m_pTaskSvr = NULL;
			return false;
		}
		m_oListeners.push_back(pListener);
	}

	//����Master�����߳�
	if(!m_pMasterThread->Start(0))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to start thread! %s:%d\n", __FILE__, __LINE__);
		return false;
	}

	//������������߳�
	if(!m_pTaskThread->Start())
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to start thread! %s:%d\n", __FILE__, __LINE__);
		return false;
	}

	//����״̬�����߳�
	if(!m_pStateSaveThread->Start())
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to start thread! %s:%d\n", __FILE__, __LINE__);
		return false;
	}

	//�����ڵ���Դ����߳�
	if(!m_pWatchThread->Start())
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to start thread! %s:%d\n", __FILE__, __LINE__);
		return false;
	}
	
	return true;
}

void CMasterNode::Stop()
{
	//ֹͣ�������
	assert(m_pTaskThread != NULL);
	m_pTaskThread->Stop();

	//ֹͣMaster�����߳�
	assert(m_pMasterThread != NULL);
	m_pMasterThread->Stop();

	//ֹͣ������������
	for(size_t i = 0; i < m_oListeners.size(); i++)
	{
		CMasterListener* pListener = m_oListeners[i];
		assert(pListener != NULL);
		pListener->Stop();
		delete pListener;
	}
	m_oListeners.clear();

	//ֹͣ�������ط���
	if(m_pTaskSvr != NULL)
	{
		sftp_server_stop(m_pTaskSvr);
		sftp_server_exit(m_pTaskSvr);
		m_pTaskSvr = NULL;
	}

	//�ر�Master������ͻ�������
	list<TClientConn*>::iterator it = m_oClientConns.begin();
	for(; it != m_oClientConns.end(); ++it)
	{
		if((*it)->nSocket != INVALID_SOCKET)
		{
			CLOSE_SOCKET((*it)->nSocket);
			FD_CLR((*it)->nSocket, &m_fdAllSet);
		}
		delete (*it);
	}
	m_oClientConns.clear();

	//�ر�Master�����׽���
	FD_CLR(m_nListenSocket, &m_fdAllSet);
	CLOSE_SOCKET(m_nListenSocket);
	m_nListenSocket = INVALID_SOCKET;
	m_nMaxSocket = INVALID_SOCKET;

	//ֹͣ��Դ���
	assert(m_pWatchThread != NULL);
	m_pWatchThread->Stop();

	//ֹͣ״̬����
	assert(m_pStateSaveThread != NULL);
	m_pStateSaveThread->Stop();
}

void CMasterNode::Dump2Html(TDumpInfoType t, string& html, int nClientID)
{
	switch(t)
	{
	case DUMP_MASTER_NODE_INFO:
		this->Dump2Html(html);
		break;
	case DUMP_RESULT_NODE_INFO:
		assert(m_pResultNodeManager != NULL);
		m_pResultNodeManager->Dump2Html(html);
		break;
	case DUMP_CLIENT_NODE_INFO:
		assert(m_pClientManager != NULL);
		m_pClientManager->Dump2Html(html);
		break;
	case DUMP_TASK_INFO:
		assert(m_pTaskManager != NULL);
		m_pTaskManager->Dump2Html(html);
		break;
	case DUMP_MASTER_LOG_INFO:
		assert(m_pLogger != NULL);
		m_pLogger->Dump2Html(html);
		break;
	case DUMP_MASTER_ERRLOG_INFO:
		assert(m_pLogger != NULL);
		m_pLogger->DumpErr2Html(html);
		break;
	case DUMP_RESULT_LOG_INFO:
		m_pResultNodeManager->DumpLog2Html(nClientID, html);
		break;
	case DUMP_RESULT_ERRLOG_INFO:
		m_pResultNodeManager->DumpErrLog2Html(nClientID, html);
		break;
	case DUMP_CLIENT_LOG_INFO:
		m_pClientManager->DumpLog2Html(nClientID, html);
		break;
	case DUMP_CLIENT_ERRLOG_INFO:
		m_pClientManager->DumpErrLog2Html(nClientID, html);
		break;
	default:
		break;
	};
}
	

void CMasterNode::WatchRutine(void* param)
{
	CMasterNode* me = (CMasterNode*)param;
#ifndef WIN32
	if(time(0) - me->m_nLastWatchTime >= me->m_pConfigure->nSourceStatusInterval)
	{
		me->m_nLastWatchTime = time(0);
		LOCK(me->m_locker);
		if(!GetSourceStatusInfo(me->m_dSourceStatus))
		{
			me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Failed to accquire the source status!\n");
		}
		UNLOCK(me->m_locker);
	}
#endif //WIN32
	Sleep(1000);
}	

void CMasterNode::MasterRutine(void *param)
{
	CMasterNode* me = (CMasterNode*)param;

	//ɾ���ѹرյ������Լ���ʱ�Ķ�����
	list<TClientConn*>::iterator it = me->m_oClientConns.begin();
	for(; it != me->m_oClientConns.end(); )
	{
		TClientConn* pClientConn = (*it);
		assert(pClientConn != NULL);

		if(pClientConn->nSocket != INVALID_SOCKET)
		{
			//��������Ƿ��ʱ
			bool bConnTimeout = false;
			switch(pClientConn->nConnType)
			{
			case SHORT_CONN_DEFAULT:
				bConnTimeout = (time(0) - pClientConn->nStartTime > MARC_SHORT_CONN_TIMEOUT);
				break;
			case LONG_CONN_HEART: 
				bConnTimeout = (time(0) - pClientConn->nLastActiveTime > MARC_HEART_TIMEOUT);
				break;
			default:
				break;
			};

			//��ʱ��ر�����
			if(bConnTimeout)
			{
				me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Connection (type=%d, %s:%d) timeout detected! close it! %s:%d\n", 
					pClientConn->nConnType, pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
				me->CloseClient(pClientConn);
			}
		}

		//ɾ���ѹرյĿͻ���
		if(pClientConn->nSocket == INVALID_SOCKET)
		{
			it = me->m_oClientConns.erase(it);
			delete pClientConn;
		}
		else
		{
			it++;
		}
	}

	//�׽��ֶ��г�ʼ��
	fd_set fdRead, fdException;
	memcpy(&fdRead, &me->m_fdAllSet, sizeof(fd_set));
	memcpy(&fdException, &me->m_fdAllSet, sizeof(fd_set));

	//select����
	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;
	int inReady = select((int)me->m_nMaxSocket+1, &fdRead, NULL, &fdException, &timeout);
	if(inReady < 0)//����
	{
		me->m_pLogger->Write(CRunLogger::LOG_ERROR, "select error: %s! %s:%d\n", strerror(errno), __FILE__, __LINE__);
		Sleep(2000);
		return ; 
	}
	if(inReady == 0)//��ʱ
	{
		//me->m_pLogger->Write(CRunLogger::LOG_INFO, "select timeout, no message need to be processed\n");
		return; 
	}

	if(FD_ISSET(me->m_nListenSocket, &fdRead))
	{
		//�¿ͻ�������
		sockaddr_in addrRemote;
		socklen_t nAddrLen = sizeof(sockaddr_in);
		SOCKET nClientSock = accept(me->m_nListenSocket,(sockaddr*)&addrRemote,&nAddrLen);
		if(nClientSock != INVALID_SOCKET)
		{
			FD_SET(nClientSock, &me->m_fdAllSet);

			//�������Ӷ���������Ӷ���
			TClientConn *pClientConn = new TClientConn;
			assert(pClientConn != NULL);
			pClientConn->sIP = inet_ntoa(addrRemote.sin_addr);
			pClientConn->nPort = ntohs(addrRemote.sin_port);
			pClientConn->nSocket = nClientSock;
			pClientConn->nStartTime = (int)time(0);
			pClientConn->nLastActiveTime = (int)time(0);
			pClientConn->nConnType = SHORT_CONN_DEFAULT;
			pClientConn->nClientID = 0;
			me->m_oClientConns.push_back(pClientConn);

			//��¼����׽���
			if(me->m_nMaxSocket < nClientSock)
				me->m_nMaxSocket = nClientSock;
		}
		else
		{
			me->m_pLogger->Write(CRunLogger::LOG_ERROR, "accept failed! %s. %s:%d\n", strerror(errno), __FILE__, __LINE__);
		}

		if(--inReady<=0) return ;
	}

	//���ղ���������ͻ��˵���Ϣ
	it = me->m_oClientConns.begin();
	for(; it!=me->m_oClientConns.end() && inReady > 0; ++it)
	{
		TClientConn *pClientConn = (*it);
		assert(pClientConn->nSocket != INVALID_SOCKET);
		if(FD_ISSET(pClientConn->nSocket, &fdException)) //�쳣
		{
			me->m_pLogger->Write(CRunLogger::LOG_WARNING, "socket exception detected, maybe connection(%s:%d) closed! %s:%d\n",
				pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
			pClientConn->nLastActiveTime = (int)time(0);
			me->CloseClient(pClientConn);
			--inReady;
		}
		else if(FD_ISSET(pClientConn->nSocket, &fdRead)) //����Ϣ��
		{
			pClientConn->nLastActiveTime = (int)time(0);
			--inReady;

			_stDataPacket recvPacket;
			memset(&recvPacket, 0, sizeof(_stDataPacket));
			int nRecev = network_recv(pClientConn->nSocket, (char*)&recvPacket, sizeof(_stDataPacket));
			switch(nRecev)
			{
			case MARC_NETWORK_OK:
				Net2Host(recvPacket);
				me->MessageHandler(pClientConn, recvPacket);
				break;
			case MARC_NETWORK_TIMEOUT:
				me->m_pLogger->Write(CRunLogger::LOG_WARNING, "network_recv timeout! close the connection(%s:%d)! %s:%d\n",
					pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
				me->CloseClient(pClientConn);
				break;
			case MARC_NETWORK_CLOSED:
				me->m_pLogger->Write(CRunLogger::LOG_WARNING, "connection(%s:%d) closed! %s:%d\n",
					pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
				me->CloseClient(pClientConn);
				break;
			case MARC_NETWORK_ERROR:
				me->m_pLogger->Write(CRunLogger::LOG_ERROR, "network error: %s! close the connection(%s:%d)! %s:%d\n",
					strerror(errno), pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
				me->CloseClient(pClientConn);
				break;
			};
		}
	}
}

void CMasterNode::MessageHandler(TClientConn *pClientConn, const _stDataPacket& recvPacket)
{
	_stDataPacket sendPacket; //���ظ��ͻ��˵���Ϣ��
	memset(&sendPacket, 0, sizeof(_stDataPacket));
	_stResultNode *pResultNode = NULL;
	_stResultStatus *pResultStatus = NULL;
	_stNodeSourceStatus *pSourceStatus = NULL;
	_stAppVerInfo dAppVerInfo;
	string sAppType  = "";
	string sInstallPath = "";
	int nResultID = 0;
	int nAppVersion = 0;
	bool bDisabled = false;
	memset(&dAppVerInfo, 0, sizeof(_stAppVerInfo));

	int nClientID = recvPacket.nClientID;
	switch(recvPacket.nCommand)
	{
	case C2M_CMD_REGISTER_REQ:	//Client�ڵ�ע�ᣬ����ʵ�ʵļ���IP�Ͷ˿�
		pClientConn->nConnType = SHORT_CONN_DEFAULT;
		ParseAppTypeAndInstallPath(recvPacket.cBuffer, sAppType, sInstallPath);
		m_pLogger->Write(CRunLogger::LOG_INFO, "ClientNode(ID=%d, AppType=%s, IP=%s) request register\n", \
			nClientID, sAppType.c_str(), pClientConn->sIP.c_str());
		if(recvPacket.nOffset == MARC_MAGIC_NUMBER) //ħ���Ϸ��Լ��
		{
			//ClientIDΪ0ʱΪ�����һ��ID
			if(nClientID == 0)
			{
				nClientID = m_nClientIDBase++;
				sendPacket.nClientID = nClientID;
				m_pLogger->Write(CRunLogger::LOG_INFO, "Assign ID=%d to the ClientNode\n", sendPacket.nClientID);
			}

			// ���ýڵ�ID�����ڻ�����ڵ��ýڵ���ʧЧ�򽫸�����С�ļ��������֪Client�ڵ�
			if(!m_pClientManager->FindClient(nClientID, bDisabled) || bDisabled)
			{
				pClientConn->nClientID = nClientID;

				//��ø�����С��Listener
				CMasterListener* pListener = SelectListener();
				assert(pListener != NULL);
				sendPacket.nCommand = M2C_CMD_REGISTER_YES;
				sendPacket.nOffset = pListener->GetListenPort();
				m_pLogger->Write(CRunLogger::LOG_INFO, "Assign Listener(Port=%d) to the ClientNode(ID=%d)\n", sendPacket.nOffset, nClientID);
			}
			else //��Client�ڵ�ID�Ѵ���������ע��
			{
				sendPacket.nCommand = M2C_CMD_REGISTER_NO;
				m_pLogger->Write(CRunLogger::LOG_WARNING, "ClientNode(ID=%d) has been registered, refuse it! %s:%d\n", nClientID, __FILE__, __LINE__);
			}
		}
		else
		{
			sendPacket.nCommand = M2C_CMD_REGISTER_NO;
			m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid ClientNode (dismatched magic number)! %s:%d\n", __FILE__, __LINE__);
		}
		Host2Net(sendPacket);
		if(network_send(pClientConn->nSocket,(char *)&sendPacket,sizeof(_stDataPacket)) != MARC_NETWORK_OK)
		{
			m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send reponse message, close the connection(%s:%d)! %s:%d\n",
				pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
			CloseClient(pClientConn);
		}
		break;
	case R2M_CMD_HEART_SEND: //Result�ڵ㷢������
		pClientConn->nConnType = LONG_CONN_HEART;
		pClientConn->nClientID = nClientID;
		pResultStatus = (_stResultStatus*)recvPacket.cBuffer;
		Net2Host(*pResultStatus);
		
		//��¼��Result�ڵ��״̬�������Ծʱ��
		m_pResultNodeManager->SetRunningStatus(nClientID, pResultStatus);
		m_pResultNodeManager->SetActiveTime(nClientID, time(0));

		//m_pLogger->Write(CRunLogger::LOG_INFO, "Heartbeat from ResultNode(ID=%d, %s:%d, overload=%d) received...\n",
		//	nClientID, pClientConn->sIP.c_str(), pClientConn->nPort, pResultStatus->nOverload);
		break;
	case R2M_CMD_SOURCE_STATUS: //�ڵ���Դʹ��״��
		pClientConn->nConnType = LONG_CONN_HEART;

		//��¼��Result�ڵ����Դʹ��״��
		pSourceStatus = (_stNodeSourceStatus*)recvPacket.cBuffer;
		Net2Host(*pSourceStatus);
		m_pResultNodeManager->SetSourceStatus(nClientID, pSourceStatus);
		break;
	case R2M_CMD_APPVER_REQ: //Result�ڵ�����App�汾����
		pClientConn->nConnType = SHORT_CONN_DEFAULT;
		sAppType = recvPacket.cBuffer;
		nAppVersion = recvPacket.nOffset;
		m_pLogger->Write(CRunLogger::LOG_INFO, "ResultNode(ID=%d, IP=%s) request for version check, current version(AppType=%s) is %d\n",
			nClientID, pClientConn->sIP.c_str(), sAppType.c_str(), nAppVersion);

		if(GetResultAppUpdateVersion(sAppType, nAppVersion, dAppVerInfo))
		{
			m_pLogger->Write(CRunLogger::LOG_INFO, "ResultNode(ID=%d, IP=%s) new application version(%d) found for AppType=%s, update-zipfile: %s\n",
				nClientID, pClientConn->sIP.c_str(), dAppVerInfo.nUpdateVersion, sAppType.c_str(), dAppVerInfo.chUpdateFile);
			sendPacket.nCommand = M2R_CMD_APPVER_YES;
			Host2Net(dAppVerInfo);
			memcpy(sendPacket.cBuffer, (char *)&dAppVerInfo, sizeof(_stAppVerInfo));
		}
		else
		{
			sendPacket.nCommand = M2R_CMD_APPVER_NO;
			m_pLogger->Write(CRunLogger::LOG_INFO, "ResultNode(ID=%d, IP=%s) needn't update application version for AppType=%s\n", 
				nClientID, pClientConn->sIP.c_str(), sAppType.c_str());
		}
		Host2Net(sendPacket);
		if(network_send(pClientConn->nSocket, (char *)&sendPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
		{
			m_pLogger->Write(CRunLogger::LOG_ERROR, "network_send failed, close the connection! %s:%d\n", __FILE__, __LINE__);
			CloseClient(pClientConn);
		}
		break;
	case R2M_CMD_ERRLOG_INFO: //Result�ڵ㷢���쳣��־��Ϣ
		m_pResultNodeManager->AddErrLog(nClientID, recvPacket.cBuffer);
		break;
	case R2M_CMD_REGISTER_REQ: //Result�ڵ�����ע�ᣬ����Ӧ����Ϣ
		pClientConn->nConnType = LONG_CONN_HEART;
		pResultNode = (_stResultNode *)recvPacket.cBuffer;
		Net2Host(*pResultNode);
		m_pLogger->Write(CRunLogger::LOG_INFO, "ResultNode(AppType=%s, %s:%d|%s) request register\n",
			pResultNode->chAppType, pResultNode->chIp, pResultNode->iPort, pResultNode->chSavePath);
		if(recvPacket.nOffset == MARC_MAGIC_NUMBER) //ħ���Ϸ��Լ��
		{
			//�ж�Result�ڵ��Ƿ��Ѵ���
			assert(m_pResultNodeManager != NULL);
			if(!m_pResultNodeManager->FindResultNode(pResultNode->chIp, pResultNode->iPort, nResultID, bDisabled))
			{
				sendPacket.nCommand = M2R_CMD_REGISTER_YES;
				sendPacket.nClientID = m_nResultIDBase++;
				m_pResultNodeManager->AddResultNode(sendPacket.nClientID, pResultNode);
				pClientConn->nClientID = sendPacket.nClientID;
				pClientConn->nPort = pResultNode->iPort;
				m_pLogger->Write(CRunLogger::LOG_INFO, "ResultNode(AppType=%s, %s:%d|%s) register successfully, assigned ID=%d\n",
					pResultNode->chAppType, pResultNode->chIp, pResultNode->iPort, pResultNode->chSavePath, sendPacket.nClientID);
			}
			else if(bDisabled) //�ҵ���ͬ��Result�ڵ㵫����ʧЧ��ָ��ýڵ�
			{
				sendPacket.nCommand = M2R_CMD_REGISTER_YES;
				sendPacket.nClientID = nResultID;
				m_pResultNodeManager->AddResultNode(sendPacket.nClientID, pResultNode);
				pClientConn->nClientID = sendPacket.nClientID;
				pClientConn->nPort = pResultNode->iPort;
				m_pLogger->Write(CRunLogger::LOG_INFO, "ResultNode(AppType=%s, %s:%d|%s) resume register, ID=%d\n",
					pResultNode->chAppType, pResultNode->chIp, pResultNode->iPort, pResultNode->chSavePath, sendPacket.nClientID);
			}
			else //�ҵ���ͬ��Result�ڵ�
			{
				sendPacket.nCommand = M2R_CMD_REGISTER_NO;
				m_pLogger->Write(CRunLogger::LOG_WARNING, "ResultNode(%s:%d) has been registered, refuse it! %s:%d\n",
					pResultNode->chIp, pResultNode->iPort, __FILE__, __LINE__);
			}
		}
		else
		{
			sendPacket.nCommand = M2R_CMD_REGISTER_NO;
			m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid ResultNode! %s:%d\n", __FILE__, __LINE__);
		}

		//�ظ�Result�ڵ�
		Host2Net(sendPacket);
		if(network_send(pClientConn->nSocket,(char *)&sendPacket,sizeof(_stDataPacket)) != MARC_NETWORK_OK)
		{
			m_pLogger->Write(CRunLogger::LOG_ERROR, "network_send failed, close the connection(%s:%d)! %s:%d\n", 
				pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
			CloseClient(pClientConn);
		}
		break;
	case R2M_CMD_INSTALL_PATH: //Result�ڵ��֪����·��
		sInstallPath = recvPacket.cBuffer;
		m_pResultNodeManager->SetInstallPath(nClientID, sInstallPath.c_str());
		m_pLogger->Write(CRunLogger::LOG_INFO, "ResultNode(ID=%d, %s:%d) install-path is %s ...\n",
			nClientID, pClientConn->sIP.c_str(), pClientConn->nPort, sInstallPath.c_str());
		break;
	case R2M_CMD_UNREGISTER: //Result�ڵ�ע��
		m_pLogger->Write(CRunLogger::LOG_WARNING, "ResultNode(ID=%d, %s:%d) logout, maybe exited humanlly\n", 
			pClientConn->nClientID, pClientConn->sIP.c_str(), pClientConn->nPort);
		CloseClient(pClientConn);
		break;
	case R2M_CMD_CLOSE: //���ӹر�
		CloseClient(pClientConn);
		break;
	case C2M_CMD_CLOSE: //���ӹر�
		CloseClient(pClientConn);
		break;
	default: //������Ϣ
		m_pLogger->Write(CRunLogger::LOG_WARNING, "Invalid message command(%d) received, close the connection(%s:%d)! %s:%d\n",
			recvPacket.nCommand, pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
		CloseClient(pClientConn);
		break;
	}
}

bool CMasterNode::GetResultAppUpdateVersion(const string& sAppType, int nCurAppVersion, _stAppVerInfo &dAppVerInfo)
{
	bool ret = false;
	LOCK(m_pConfigure->locker4ResultApp);
	map<string, CAppUpdateInfo*>::iterator it = m_pConfigure->oResultAppVersion.find(sAppType);
	if(it != m_pConfigure->oResultAppVersion.end())
	{
		CAppUpdateInfo* p = it->second;
		assert(p != NULL);
		assert(p->sAppType == sAppType);
		if(p->nAppUpdateVer > nCurAppVersion)
		{
			if(DIR_EXIST(p->sAppUpdateFile.c_str()))
			{
				dAppVerInfo.usPort = m_pConfigure->nTaskPort;
				dAppVerInfo.nUpdateVersion = p->nAppUpdateVer;
				strcpy(dAppVerInfo.chUpdateFile, p->sAppUpdateFile.c_str());
				ret = true;
			}
			else
			{
				m_pLogger->Write(CRunLogger::LOG_ERROR, "update-zipfile not existed: %s\n", p->sAppUpdateFile.c_str());
			}
		}
	}
	UNLOCK(m_pConfigure->locker4ResultApp);
	return ret;
}

int CMasterNode::GetActiveConns()
{
	int nActiveConns = 0;
	nActiveConns += m_oClientConns.size();
	nActiveConns += sftp_server_active_conns(m_pTaskSvr);
	for(size_t i = 0; i < m_oListeners.size(); i++)
	{
		nActiveConns += m_oListeners[i]->GetActiveConns();
	}
	return nActiveConns;
}

CMasterListener* CMasterNode::SelectListener()
{
	assert(!m_oListeners.empty());
	CMasterListener* pListener = m_oListeners[0];
	for(size_t i=1; i < m_oListeners.size(); i++)
	{
		if(m_oListeners[i]->GetActiveConns() < pListener->GetActiveConns())
		{
			pListener = m_oListeners[i];
		}
	}
	return pListener;
}

void CMasterNode::CloseClient(TClientConn *pClientConn)
{
	//��������Ϊ����������˵��Result�ڵ�Ͽ��ˣ���ɾ����Result�ڵ�
	if(pClientConn->nConnType == LONG_CONN_HEART)
	{
		m_pResultNodeManager->RemoveResultNode(pClientConn->nClientID);
	}

	CLOSE_SOCKET(pClientConn->nSocket);
	FD_CLR(pClientConn->nSocket, &m_fdAllSet);
	pClientConn->nSocket = INVALID_SOCKET;
}

void CMasterNode::TaskRutine(void* param)
{
	//���̺߳���������������
	CMasterNode* me = (CMasterNode*)param;

	//ȡ���贴�������Client�ڵ��ID����Ӧ�ó�������
	vector<int> oFreeClientIDs;
	vector<string> oFreeClientAppTypes;
	me->m_pClientManager->GetClientsOfNeedCreateTask(oFreeClientIDs, oFreeClientAppTypes);
	if(oFreeClientIDs.empty())
	{
		Sleep(1000);
		return ;
	}

	//Ϊÿ��û�д����������Client�ڵ���������
	assert(oFreeClientIDs.size() == oFreeClientAppTypes.size());
	for(size_t i=0; i < oFreeClientIDs.size(); i++)
	{
		int nClientID = oFreeClientIDs[i];
		const string& sClientAppType = oFreeClientAppTypes[i];

		//ִ���������ɳ�������ݸ����ڵ���ϴ����񴴽�ʧ��ʱ���жϱ����Ƿ�ҪΪ�䴴������
		map<int,int>::iterator it = me->m_oClientID2LastTaskCreateTime.find(nClientID);
		if(it != me->m_oClientID2LastTaskCreateTime.end())
		{
			if(time(0) - it->second < MARC_TASKCREATE_TIME_INTERVAL)
			{
				continue;
			}
		}
		if(!me->ExecTaskCreateApp(nClientID, sClientAppType))
		{
			me->m_oClientID2LastTaskCreateTime[nClientID] = (int)time(0);
		}
	}

	//��������ļ���
	CleanDir(me->m_pConfigure->sTaskPath.c_str());
}

bool CMasterNode::ExecTaskCreateApp(int nClientID, const string& sAppType)
{
	time_t t1 = time(0);

	//���������������
	string sAppCmd = "";
	if(!GetAppCmd(sAppType, sAppCmd)) return false;
	if(sAppCmd.empty()) return false;

    //�����������ļ���·��
	string sCreateTime = formatDateTime(time(0),1);
	char chTaskDirName[512] = {0};
	sprintf(chTaskDirName, "%d_%s_task_%s", nClientID, sAppType.c_str(), sCreateTime.c_str());
	char sTaskDirPath[1024] = {0};
	sprintf(sTaskDirPath, "%s%s/", m_pConfigure->sTaskPath.c_str(), chTaskDirName);
	NormalizePath(sTaskDirPath, false);
	if(!CreateFilePath(sTaskDirPath))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't create the file path: %s, %s:%d\n", sTaskDirPath, __FILE__, __LINE__);
		return false;
	}

	//�����������ɳ�����������ʽ�����ó�ʱ�����ӿ�: [AppCmd] [TaskDirPath] [ClientID] 
	//��"./NewsTaskCreate ./task/1_NewsGather_task_20100204095923/ 1"
	char chCmd[1024] = {0};
	sprintf(chCmd, "%s %s %d", sAppCmd.c_str(), sTaskDirPath, nClientID);
	NormalizePath(chCmd, false);
	m_pLogger->Write(CRunLogger::LOG_INFO, "Create task for ClientNode(ID=%d, AppType=%s), command: %s\n", nClientID, sAppType.c_str(), chCmd);
	if(!Exec(chCmd, m_pConfigure->nAppRunTimeout))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to execute command: %s, %s:%d\n", chCmd, __FILE__, __LINE__);
		deleteDir(sTaskDirPath);
		return false;
	}

	//����Ƿ�ִ�гɹ���ִ�гɹ�������sTaskDirPath�ļ����д���.success��־�ļ���
	char sAppFlagFile[1024] = {0};
	sprintf(sAppFlagFile, "%s.success", sTaskDirPath);
	if(!DIR_EXIST(sAppFlagFile))
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "Flag file '%s' not found, appliation program '%s' exited abnormally! %s:%d\n", 
			sAppFlagFile, sAppCmd.c_str(), __FILE__, __LINE__);
		deleteDir(sTaskDirPath);
		return false;
	}

	//ɾ��.success��־�ļ�������ļ����Ƿ��
	deleteFile(sAppFlagFile);
	CDirScanner ds(sTaskDirPath, false);
	const vector<string>& oFileList = ds.GetAllList();
	if(oFileList.empty())
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "No data file found in folder %s! %s:%d\n",
			sTaskDirPath, __FILE__, __LINE__);
		return false;
	}

	//ѹ�������ļ���
	string sTaskZipFilePath = "";
	if(!MyZipTask(chTaskDirName, sTaskZipFilePath))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "myzip failed: %s, %s:%d\n", chTaskDirName, __FILE__, __LINE__);
		deleteDir(sTaskDirPath);
		return false;
	}

	//���ѹ���ļ��Ƿ����
	if(!DIR_EXIST(sTaskZipFilePath.c_str()))
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "task-zipfile not found: %s, maybe myzip failed. [%s:%d]\n", sTaskZipFilePath.c_str(), __FILE__, __LINE__);
		return false;
	}

	//����������������
	time_t t2 = time(0);
	if(!m_pTaskManager->AddTask(nClientID, sAppType, sTaskZipFilePath, t2-t1)) 
	{
		//�޷����뵽�������
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't add the task-zipfile to task queue, ignore it:%s, %s:%d\n",
			sTaskZipFilePath.c_str(), __FILE__, __LINE__);
		deleteDir(sTaskDirPath);
		return false;
	}
	m_pLogger->Write(CRunLogger::LOG_INFO, "Task created successfully for ClientNode(ID=%d, AppType=%s), task-zipfile is %s\n", 
		nClientID, sAppType.c_str(), sTaskZipFilePath.c_str());

	return true;
}

bool CMasterNode::GetAppCmd(const string& sAppType, string& sAppCmd)
{
	//�Ȳ�ѯ�ڴ����Ƿ�����
	map<string,string>::const_iterator it = m_pConfigure->oAppType2AppCmd.find(sAppType);
	if(it != m_pConfigure->oAppType2AppCmd.end())
	{
		sAppCmd = it->second;
		return true;
	}

	//�������ļ��ж�ȡ
	if(!INI::CMyIniFile::ReadIniStr(MARC_MASTER_CONF_FILE, "appcmd", sAppType.c_str(), sAppCmd) || sAppCmd.empty())
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "Can't find application command for '%s' in configure file '%s'! %s:%d\n", 
			sAppType.c_str(), MARC_MASTER_CONF_FILE, __FILE__, __LINE__);
	}
	NormalizePath(sAppCmd, false);
	m_pConfigure->oAppType2AppCmd[sAppType] = sAppCmd;
	return true;
}

bool CMasterNode::MyZipTask(const string& sTaskDirName, string& sTaskZipFilePath)
{
	sTaskZipFilePath = m_pConfigure->sZipTaskPath + sTaskDirName + ".myzip";
	char chCmd[1024] = {0};
#ifdef WIN32
	sprintf(chCmd, "%s zip %s%s/ %s", 
		MARC_MYZIP_APPCMD, 
		m_pConfigure->sTaskPath.c_str(),
		sTaskDirName.c_str(),
		sTaskZipFilePath.c_str());
#else
	sprintf(chCmd, "/bin/tar -C %s%s/ ./ -czf %s",  
		m_pConfigure->sTaskPath.c_str(),
		sTaskDirName.c_str(),
		sTaskZipFilePath.c_str());
#endif
	NormalizePath(chCmd, false);
	m_pLogger->Write(CRunLogger::LOG_INFO, "myzip task: %s\n", chCmd);
	if(!Exec(chCmd))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to execute command: %s, %s:%d\n", chCmd, __FILE__, __LINE__);
		return false;
	}
	return true;
}

void CMasterNode::OnTaskDownloaded(CMasterNode* me, const char* sTaskZipFilePath)
{
	//me->m_pLogger->Write(CRunLogger::LOG_INFO, "Task downloaded: %s\n", sTaskZipFilePath);
}

void CMasterNode::StateSaveRutine(void* param)
{
	CMasterNode *me = (CMasterNode *)param;

#if 0
	//ÿ��nMaxSaveStateTime�뱣��һ��
	if(me->m_oListeners.empty()) return ;
	if(time(0) - me->m_nLastSaveTime < me->m_pConfigure->nMaxSaveStateTime) return ; 
	me->m_nLastSaveTime = (int)time(0);

	//ִ�и���Listener��״̬���溯��
	string sCurTime = formatDateTime(time(0), 1);
	vector<string> oStateFiles;
	me->m_pClientManager->SaveClientState(sCurTime, oStateFiles);

	if(oStateFiles.empty())
	{
		return ;
	}

	//������״̬�ļ���д��.list�ļ�
	char chStateFileList[256] = {0};
	sprintf(chStateFileList, "./state/%s.list", sCurTime.c_str());
	NormalizePath(chStateFileList, false);
	FILE *fpStateFileList = fopen(chStateFileList, "wb");
	assert(fpStateFileList != NULL);
	for(size_t j = 0; j < oStateFiles.size(); j++)
	{
		fprintf(fpStateFileList, "%s\n", oStateFiles[j].c_str());
	}
	fclose(fpStateFileList);

	//��ֹ��д��ͻ,��Ҫдһ���������
	char chStateFlagFile[256] = {0};
	sprintf(chStateFlagFile, "./state/%s.ok", sCurTime.c_str());
	NormalizePath(chStateFlagFile, false);
	if(!CreateFlagFile(chStateFlagFile))
	{
		me->m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't create flag file '%s'!\n", chStateFlagFile);
	}
#endif
}

void CMasterNode::Dump2Html(string& html)
{
	html = "<html><head></head>";
	html += "<script type=\"text/javascript\" src=\"marc.js\"></script>";
	html += "<link type=\"text/css\" rel=\"stylesheet\" href=\"marc.css\" />";
	html += "<body>";
	
	time_t tRunTime = time(0) - m_pConfigure->nStartupTime;
	int nRunDays = tRunTime/(24*3600);
	int nRunHours = (tRunTime - nRunDays*24*3600)/3600;
	int nRunMinutes = (tRunTime - nRunDays*24*3600 - nRunHours*3600)/60;
	char buf[1024] = {0};
	_snprintf(buf, sizeof(buf), "<div style=\"position: absolute; width: 1100px; height: 470px; z-index: 1; left: 80px; top: 25px\" id=\"layer1\">"); html += buf;
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/love.png\"><b>&nbsp;������ַ: </b>%s:%d��<b>�ڵ���Ŀ: </b>Result�ڵ� %d����Client�ڵ� %d��</p></font>\n",m_pConfigure->sIP.c_str(),m_pConfigure->nPort,m_pResultNodeManager->NodeCount(), m_pClientManager->NodeCount()); html += buf;
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/home.png\"><b>&nbsp;����λ��: </b>%s\n</p></font>", m_pConfigure->sInstallPath.c_str()); html += buf;
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/clock.png\"><b>&nbsp;����ʱ��: </b>%s �������� %d �� %d Сʱ %d ���ӡ�</p></font>\n", formatDateTime(m_pConfigure->nStartupTime).c_str(), nRunDays, nRunHours, nRunMinutes); html += buf;
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/wifi.png\"><b>&nbsp;��Ծ������: </b>��ǰ�ܹ�%d����Ծ���ӣ�����%d��������������</p></font>\n", GetActiveConns(), sftp_server_active_conns(m_pTaskSvr)); html += buf;
	
	LOCK(m_pTaskStatInfo->m_locker);
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/info.png\"><b>&nbsp;����������: </b>%d��, ƽ������ʱ��: %d��</p></font>\n", m_pTaskStatInfo->nTotalCreatedTasks, m_pTaskStatInfo->nTotalCreatedTasks==0?0:m_pTaskStatInfo->nTotalCreatedTimeUsed/m_pTaskStatInfo->nTotalCreatedTasks); html += buf;
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/task.png\"><b>&nbsp;�����������: </b>ID=%d, �����ļ�Ϊ%s</p></font>\n", m_pTaskStatInfo->nLastCreatedTaskID, m_pTaskStatInfo->sLastCreatedTaskFile.c_str()); html += buf;
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/wifi.png\"><b>&nbsp;����ִ�����: </b>�ַ�������: %d��, ���������: %d��, ʧ��������: %d��</p></font>\n", m_pTaskStatInfo->nTotalDeliveredTasks, m_pTaskStatInfo->nTotalFinishdTasks, m_pTaskStatInfo->nTotalFailedTasks); html += buf;
	UNLOCK(m_pTaskStatInfo->m_locker);
	
	LOCK(m_locker);
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/activity.png\"><b>&nbsp;��Դʹ�����: </b>CPU ������: %d%%, ����ʣ���� %d%%, �ڴ������ %d%%, ��������: %d bps (��ȡʱ��: %s)</p></font>\n", m_dSourceStatus.cpu_idle_ratio, m_dSourceStatus.disk_avail_ratio, m_dSourceStatus.memory_avail_ratio, m_dSourceStatus.nic_bps, formatDateTime(m_dSourceStatus.watch_timestamp).c_str()); html += buf;
	UNLOCK(m_locker);

	html += "</body></html>";
}
