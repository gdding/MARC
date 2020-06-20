#include "MasterListener.h"
#include "ResultNodeManager.h"
#include "TaskManager.h"
#include "ClientManager.h"
#include "../utils/Utility.h"
#include "../utils/Network.h"
#include "../utils/LoopThread.h"
#include "../utils/RunLogger.h"


CMasterListener::CMasterListener(CMasterConf* pServerConf, 
								 CTaskManager* pTaskManager,
								 CResultNodeManager* pResultNodeManager,
								 CClientManager* pClientManager,
								 CRunLogger* pLogger)
{
	m_pConfigure = pServerConf;
	m_pTaskManager = pTaskManager;
	m_pResultNodeManager = pResultNodeManager;
	m_pClientManager = pClientManager;
	m_pLogger = pLogger;
	m_sListenIP = "";
	m_nListenPort = 0;
	m_nListenSocket = INVALID_SOCKET;
	m_nMaxSocket = INVALID_SOCKET;	

	m_pListenThread = new CLoopThread();
	m_pListenThread->SetRutine(ListenRutine,this);
}

CMasterListener::~CMasterListener()
{
	if(m_pListenThread != NULL)
		delete m_pListenThread;
}

bool CMasterListener::Start(const char *sListenIP,unsigned short nListenPortBase, int nBackLog)
{
	static unsigned short nPortInc = 1;
	assert(sListenIP != NULL);
	assert(nBackLog > 0);
	m_sListenIP = sListenIP;

	//socket��ʼ��(nBackLogΪ֧�ֵĲ���������)
	//Ϊ����˿ڱ�ռ�ã�ʵ��ʹ�õĶ˿ڲ��̶�
	m_nListenSocket = INVALID_SOCKET;
	while(m_nListenSocket == INVALID_SOCKET)
	{
		m_nListenPort = nListenPortBase + (nPortInc++);
		m_nListenSocket = network_listener_init(sListenIP, m_nListenPort, nBackLog);
	}
	m_nMaxSocket = m_nListenSocket;

	//��ʼ��fdset
	FD_ZERO(&m_fdAllSet);
	FD_SET(m_nListenSocket, &m_fdAllSet);

	//���������߳�
	assert(m_pListenThread != NULL);
	if(!m_pListenThread->Start(0))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to start thread! %s:%d\n", __FILE__, __LINE__);
		return false;
	}

	//m_pLogger->Write(CRunLogger::LOG_INFO, "Listener startup successfully, listening on %s:%d\n", sListenIP, m_nListenPort);
	return true;
}

void CMasterListener::Stop()
{
	//��ֹ�����߳�
	assert(m_pListenThread != NULL);
	m_pListenThread->Stop();

	//�رո���Client�ڵ�����
	list<TClientConn*>::iterator it = m_oClientConns.begin();
	for(; it != m_oClientConns.end(); ++it)
	{
		TClientConn* pClientConn = (*it);
		assert(pClientConn != NULL);
		if(pClientConn->nSocket != INVALID_SOCKET)
			CLOSE_SOCKET(pClientConn->nSocket);
		delete pClientConn;
	}

	//�رռ����׽���
	CLOSE_SOCKET(m_nListenSocket);
	FD_CLR(m_nListenSocket, &m_fdAllSet);
	FD_ZERO(&m_fdAllSet);
	m_nListenSocket = INVALID_SOCKET;
	m_nMaxSocket = INVALID_SOCKET;
}

void CMasterListener::ListenRutine(void* param)
{
	//ȡ���̲߳���
	CMasterListener *me = (CMasterListener*)param;

	//ɾ���ѹرյ������Լ���ʱ�Ķ�����
	list<TClientConn*>::iterator it = me->m_oClientConns.begin();
	for(; it != me->m_oClientConns.end(); )
	{
		TClientConn* pClientConn = (*it);
		assert(pClientConn != NULL);

		//��������Ƿ��ʱ
		bool bConnTimeout = false;
		switch(pClientConn->nConnType)
		{
		case SHORT_CONN_DEFAULT:
			bConnTimeout = (time(0) - pClientConn->nStartTime > MARC_SHORT_CONN_TIMEOUT);
			break;
		case SHORT_CONN_TASKREQ:
			bConnTimeout = (time(0) - pClientConn->nStartTime > MARC_SHORT_CONN_TIMEOUT);
			break;
		case SHORT_CONN_UPLOADREQ:
			bConnTimeout = (time(0) - pClientConn->nStartTime > MARC_SHORT_CONN_TIMEOUT);
			break;
		case LONG_CONN_HEART: 
			//��������Ƿ�ʱ
			bConnTimeout = (time(0) - pClientConn->nLastActiveTime > MARC_HEART_TIMEOUT);
			if(bConnTimeout)
			{
				me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Heartbeat from socket(%s:%d) timeout detected! %s:%d\n", 
					pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
			}
			break;
		default:
			bConnTimeout = false;
			break;
		};

		//��ʱ��ر�����
		if(bConnTimeout)
		{
			me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Connection (type=%d, %s:%d) timeout detected! close it! %s:%d\n", 
				pClientConn->nConnType, pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
			me->CloseClient(pClientConn);
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
	int inReady = select((int)me->m_nMaxSocket +1, &fdRead, NULL, &fdException, &timeout);
	if(inReady < 0)//����
	{
		me->m_pLogger->Write(CRunLogger::LOG_ERROR, "select error: %s! %s:%d\n", strerror(errno), __FILE__, __LINE__);
		Sleep(2000);
		return ; 
	}
	if(inReady == 0)//��ʱ�����账��
	{
		//me->m_pLogger->Write(CRunLogger::LOG_INFO, "select timeout, no message need to be processed\n");
		return; 
	}

	//���������ӣ�����ӵ�Client�ڵ����Ӷ�����
	if(FD_ISSET(me->m_nListenSocket, &fdRead))
	{
		//�õ�Client�ڵ�socket
		sockaddr_in addrRemote;
		socklen_t slen = sizeof(sockaddr_in);
		SOCKET nClientSock = accept(me->m_nListenSocket,(sockaddr*)&addrRemote,&slen);
		if(nClientSock != INVALID_SOCKET)
		{
			FD_SET(nClientSock, &me->m_fdAllSet);

			//���µ�Client�ڵ�������Ӷ���
			TClientConn* pClientConn = new TClientConn;
			assert(pClientConn != NULL);
			pClientConn->sIP = inet_ntoa(addrRemote.sin_addr);
			pClientConn->nPort = ntohs(addrRemote.sin_port);
			pClientConn->nSocket = nClientSock;
			pClientConn->nStartTime = (int)time(0);
			pClientConn->nLastActiveTime = (int)time(0);
			pClientConn->nConnType = SHORT_CONN_DEFAULT;
			pClientConn->nClientID = 0;
			me->m_oClientConns.push_back(pClientConn);
			//me->m_pLogger->Write(CRunLogger::LOG_INFO, "New connection(%s:%d), add to connection queue\n", pClientConn->sIP.c_str(), pClientConn->nPort);

			//��¼����׽���
			if(me->m_nMaxSocket < nClientSock)
				me->m_nMaxSocket = nClientSock;
		}
		else
		{
			me->m_pLogger->Write(CRunLogger::LOG_ERROR, "Accept failed! %s. %s:%d\n", strerror(errno), __FILE__, __LINE__);
		}

		if(--inReady <= 0) return ;
	}

	//���ղ��������Client�ڵ����Ϣ
	it = me->m_oClientConns.begin();
	for(; it != me->m_oClientConns.end() && inReady > 0; ++it)
	{
		TClientConn* pClientConn = (*it);
		assert(pClientConn != NULL);
		assert(pClientConn->nSocket != INVALID_SOCKET);
		if(FD_ISSET(pClientConn->nSocket, &fdException)) //�쳣
		{
			me->m_pLogger->Write(CRunLogger::LOG_WARNING, "socket(%s:%d) exception detected, maybe closed by ClientNode! %s:%d\n", 
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

void CMasterListener::MessageHandler(TClientConn *pClientConn, const _stDataPacket& recvPacket)
{
	string sClientAppType = "";
	string sInstallPath = "";
	_stClientState dAppState;
	map<int, CClientInfo*>::iterator it;
	int nTaskID = 0;
	int nAppVersion = 0;
	bool bDisabled = false;
	_stDataPacket sendPacket; //Ӧ����Ϣ��
	_stResultNodeAddr rstSvrAddr; //Result�ڵ��ַ
	_stTaskReqInfo task;
	_stAppVerInfo dAppVerInfo;
	_stResultNode oResultNode;
	_stClientStatus* pClientStatus = NULL;
	_stNodeSourceStatus* pSourceStatus = NULL;
	memset(&sendPacket, 0 , sizeof(_stDataPacket));
	memset(&rstSvrAddr, 0, sizeof(_stResultNodeAddr));
	memset(&task, 0, sizeof(_stTaskReqInfo));

	int nClientID = recvPacket.nClientID; //Client�ڵ�ID
	pClientConn->nClientID = nClientID;
	m_pClientManager->SetActiveTime(nClientID, time(0)); //��¼�ڵ��Ծʱ��
	switch(recvPacket.nCommand)
	{
	case C2L_CMD_HEART_SEND: //Client�ڵ�����
		pClientConn->nConnType = LONG_CONN_HEART;

		//��¼��Result�ڵ��״̬
		pClientStatus = (_stClientStatus*)recvPacket.cBuffer;
		Net2Host(*pClientStatus);
		m_pClientManager->SetRunningStatus(nClientID, pClientStatus);

		//m_pLogger->Write(CRunLogger::LOG_INFO, "Heartbeat from ClientNode(ID=%d, IP=%s) received...\n", nClientID, pClientConn->sIP.c_str());

		if(!m_pClientManager->FindClient(nClientID, bDisabled) || bDisabled)
		{
			m_pLogger->Write(CRunLogger::LOG_WARNING, "ClientNode(ID=%d, IP=%s) disabled or not existed, close the connection! %s:%d\n",
				nClientID, pClientConn->sIP.c_str(), __FILE__, __LINE__);
			CloseClient(pClientConn);
		}
		break;
	case C2L_CMD_SOURCE_STATUS: //�ڵ���Դʹ��״��
		pClientConn->nConnType = LONG_CONN_HEART;

		//��¼��Client�ڵ����Դʹ��״��
		pSourceStatus = (_stNodeSourceStatus*)recvPacket.cBuffer;
		Net2Host(*pSourceStatus);
		m_pClientManager->SetSourceStatus(nClientID, pSourceStatus);
		break;
	case C2L_CMD_ERRLOG_INFO: //Client�ڵ㷢���쳣��־
		m_pClientManager->AddErrLog(nClientID, recvPacket.cBuffer);
		break;
	case C2L_CMD_REGISTER_REQ: //ע��
		pClientConn->nConnType = LONG_CONN_HEART;
		ParseAppTypeAndInstallPath(recvPacket.cBuffer, sClientAppType, sInstallPath);

		//����ע������Ӧ����Ϣ
		memset(&sendPacket, 0, sizeof(_stDataPacket));
		if(recvPacket.nOffset != MARC_MAGIC_NUMBER)
		{
			sendPacket.nCommand = L2C_CMD_INVALID_CLIENT;
			m_pLogger->Write(CRunLogger::LOG_WARNING, "Invalid ClientNode (dismatched magic number)! %s:%d\n", __FILE__, __LINE__);
		}
		else if(!m_pClientManager->FindClient(nClientID, bDisabled) || bDisabled)
		{
			sendPacket.nCommand = L2C_CMD_REGISTER_YES;
			m_pClientManager->AddClient(nClientID, sClientAppType, pClientConn->sIP, sInstallPath);
		}
		else
		{
			sendPacket.nCommand = L2C_CMD_REGISTER_NO;
			m_pLogger->Write(CRunLogger::LOG_WARNING, "ClientID(%d) has been registered, refuse it! %s:%d\n", nClientID, __FILE__, __LINE__);
		}
		Host2Net(sendPacket);
		if(network_send(pClientConn->nSocket, (char *)&sendPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
		{
			m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send response message, close the connection(%s:%d)! %s:%d\n", 
				pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
			CloseClient(pClientConn);
		}
		break;
	case C2L_CMD_STATE_SEND:  //Client�ڵ㷢��״̬��Ϣ
		memcpy(&dAppState, recvPacket.cBuffer, sizeof(_stClientState));
		m_pClientManager->UpdateClientState(nClientID, dAppState);
		break;
	case C2L_CMD_TASK_REQ: //Client�ڵ���������,����������Ϣ
		pClientConn->nConnType = SHORT_CONN_TASKREQ;
		sClientAppType = recvPacket.cBuffer;
		m_pClientManager->SetTaskRequested(nClientID, true);
		m_pLogger->Write(CRunLogger::LOG_INFO, "ClientNode(ID=%d, AppType=%s, IP=%s) request task...\n", 
			nClientID, sClientAppType.c_str(), pClientConn->sIP.c_str());
		
		if(!m_pClientManager->FindClient(nClientID, bDisabled) || bDisabled)
		{
			sendPacket.nCommand = L2C_CMD_INVALID_CLIENT;
			m_pLogger->Write(CRunLogger::LOG_WARNING, "ClientNode(ID=%d, AppType=%s, IP=%s) disabled or not existed! %s:%d\n",
				nClientID, sClientAppType.c_str(), pClientConn->sIP.c_str(), __FILE__, __LINE__);
		}
		else
		{
			//�Ӹ�Client�ڵ��������ȡ�����񣬵õ�������Ϣ
			if(GetTaskInfo(nClientID, sClientAppType, task))
			{
				m_pLogger->Write(CRunLogger::LOG_INFO, "Task(TaskID=%d, TaskFile=%s) assigned to the ClientNode\n", task.nTaskID, task.chTaskFile);
				sendPacket.nCommand = L2C_CMD_TASK_YES;
				Host2Net(task);
				memcpy(sendPacket.cBuffer, (char *)&task, sizeof(_stTaskReqInfo));
			}
			else //û�д���������
			{
				sendPacket.nCommand = L2C_CMD_TASK_NO;
				m_pLogger->Write(CRunLogger::LOG_INFO, "No task found for the ClientNode(ID=%d, AppType=%s, IP=%s)\n", 
					nClientID, sClientAppType.c_str(), pClientConn->sIP.c_str());
			}
		}
		Host2Net(sendPacket);
		if(network_send(pClientConn->nSocket, (char *)&sendPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
		{
			m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send response message, close the connection(%s:%d)! %s:%d\n",
				pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
			CloseClient(pClientConn);
		}
		break;
	case C2L_CMD_UPLOAD_REQ: //Client�ڵ������ϴ����,��Result�ڵ���Ϣ����Client�ڵ�
		pClientConn->nConnType = SHORT_CONN_UPLOADREQ;
		sClientAppType = recvPacket.cBuffer;
		m_pLogger->Write(CRunLogger::LOG_INFO, "ClientNode(ID=%d, AppType=%s, IP=%s) request to upload result...\n",
			nClientID, sClientAppType.c_str(), pClientConn->sIP.c_str());
		/*if(!m_pClientManager->FindClient(nClientID, bDisabled) || bDisabled)
		{
			sendPacket.nCommand = L2C_CMD_INVALID_CLIENT;
			m_pLogger->Write(CRunLogger::LOG_WARNING, "ClientNode(ID=%d, AppType=%s, IP=%s) disabled or not existed! %s:%d\n",
				nClientID, sClientAppType.c_str(), pClientConn->sIP.c_str(), __FILE__, __LINE__);
		}
		else*/
		{
			//ѡ��������С��Result�ڵ�
			assert(m_pResultNodeManager != NULL);
			if(m_pResultNodeManager->SelectResultNode(sClientAppType, &oResultNode))
			{
				m_pLogger->Write(CRunLogger::LOG_INFO, "ResultNode(%s:%d|%s) selected\n",
					oResultNode.chIp, oResultNode.iPort, oResultNode.chSavePath);
				sendPacket.nCommand = L2C_CMD_UPLOAD_YES;
				strcpy(rstSvrAddr.chIp, oResultNode.chIp);
				rstSvrAddr.usPort = oResultNode.iPort;
				strcpy(rstSvrAddr.chSavePath, oResultNode.chSavePath);
				Host2Net(rstSvrAddr);
				memcpy(sendPacket.cBuffer, (char *)&rstSvrAddr, sizeof(_stResultNodeAddr));
			}
			else //�Ҳ���Result�ڵ�
			{
				sendPacket.nCommand = L2C_CMD_UPLOAD_NO;
				m_pLogger->Write(CRunLogger::LOG_WARNING, "No available ResultNode! %s:%d\n", __FILE__, __LINE__);
			}
		}
		Host2Net(sendPacket);
		if(network_send(pClientConn->nSocket, (char *)&sendPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
		{
			m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send response message, close the connection! %s:%d\n", __FILE__, __LINE__);
			CloseClient(pClientConn);
		}
		break;
	case C2L_CMD_TASKDOWN_SUCCESS: //Client�ڵ��������سɹ�
		nTaskID = recvPacket.nOffset;
		sClientAppType = recvPacket.cBuffer;
		m_pLogger->Write(CRunLogger::LOG_INFO, "ClientNode(ID=%d, AppType=%s, IP=%s) downloaded the task(TaskID=%d) successfully\n",
				nClientID, sClientAppType.c_str(), pClientConn->sIP.c_str(), nTaskID);
		m_pTaskManager->SetTaskFetched(nClientID, nTaskID);
		m_pClientManager->SetTaskID(nClientID, nTaskID); //��¼Client�ڵ�ĵ�ǰ����ID
		m_pClientManager->SetTaskRequested(nClientID, false);
		break;
	case C2L_CMD_TASKDOWN_FAILED: //Client�ڵ���������ʧ��
		nTaskID = recvPacket.nOffset;
		sClientAppType = recvPacket.cBuffer;
		m_pLogger->Write(CRunLogger::LOG_WARNING, "ClientNode(ID=%d, AppType=%s, IP=%s) failed to download the task(TaskID=%d), which will be downloaded again later! %s:%d\n",
				nClientID, sClientAppType.c_str(), pClientConn->sIP.c_str(), nTaskID, __FILE__, __LINE__);
		m_pTaskManager->SetTaskFailedByDownload(nClientID, nTaskID);

		//��Client�ڵ������ID��λ
		m_pClientManager->SetTaskID(nClientID, 0);
		break;
	case C2L_CMD_APP_FINISHED: //Client�ڵ����������������
		nTaskID = recvPacket.nOffset; //nOffset��ʱ��Ÿý����Ӧ������ID
		sClientAppType = recvPacket.cBuffer;
		m_pLogger->Write(CRunLogger::LOG_INFO, "ClientNode(ID=%d, AppType=%s, IP=%s) finished Application program successfully\n",
				nClientID, sClientAppType.c_str(), pClientConn->sIP.c_str());
		m_pTaskManager->SetTaskFinished(nClientID, nTaskID);

		//��Client�ڵ������ID��λ
		m_pClientManager->SetTaskID(nClientID, 0);
		break;
	case C2L_CMD_APP_TIMEOUT: //Client�ڵ�������г�ʱ
		nTaskID = recvPacket.nOffset; //nOffset��ʱ��Ÿý����Ӧ������ID
		sClientAppType = recvPacket.cBuffer;
		m_pLogger->Write(CRunLogger::LOG_WARNING, "ClientNode(ID=%d, AppType=%s, IP=%s) Application program running timeout! %s:%d\n",
				nClientID, sClientAppType.c_str(), pClientConn->sIP.c_str(), __FILE__, __LINE__);
		m_pTaskManager->SetTaskFailed(nClientID, nTaskID);

		//��Client�ڵ������ID��λ
		m_pClientManager->SetTaskID(nClientID, 0);
		break;
	case C2L_CMD_APP_FAILED: //Client�ڵ��������ʧ��
		nTaskID = recvPacket.nOffset; //nOffset��ʱ��Ÿý����Ӧ������ID
		sClientAppType = recvPacket.cBuffer;
		m_pLogger->Write(CRunLogger::LOG_WARNING, "ClientNode(ID=%d, AppType=%s, IP=%s) Application program running failed! %s:%d\n",
				nClientID, sClientAppType.c_str(), pClientConn->sIP.c_str(), __FILE__, __LINE__);
		m_pTaskManager->SetTaskFailed(nClientID, nTaskID);

		//��Client�ڵ������ID��λ
		m_pClientManager->SetTaskID(nClientID, 0);
		break;
	case C2L_CMD_APPVER_REQ: //Client�ڵ�����汾�˲�
		pClientConn->nConnType = SHORT_CONN_DEFAULT;
		sClientAppType = recvPacket.cBuffer;
		nAppVersion = recvPacket.nOffset;
		m_pLogger->Write(CRunLogger::LOG_INFO, "ClientNode(ID=%d, AppType=%s, IP=%s) request for version check, its current version is %d\n",
			nClientID, sClientAppType.c_str(), pClientConn->sIP.c_str(), nAppVersion);

		if(GetClientAppUpdateVersion(sClientAppType, nAppVersion, dAppVerInfo))
		{
			m_pLogger->Write(CRunLogger::LOG_INFO, "ClientNode(ID=%d, AppType=%s, IP=%s) new application version(%d) found, update-zipfile: %s\n",
				nClientID, sClientAppType.c_str(), pClientConn->sIP.c_str(), dAppVerInfo.nUpdateVersion, dAppVerInfo.chUpdateFile);
			sendPacket.nCommand = L2C_CMD_APPVER_YES;
			Host2Net(dAppVerInfo);
			memcpy(sendPacket.cBuffer, (char *)&dAppVerInfo, sizeof(_stAppVerInfo));
		}
		else
		{
			sendPacket.nCommand = L2C_CMD_APPVER_NO;
			m_pLogger->Write(CRunLogger::LOG_INFO, "ClientNode(ID=%d, AppType=%s, IP=%s) needn't update application version\n", 
				nClientID, sClientAppType.c_str(), pClientConn->sIP.c_str());
		}
		Host2Net(sendPacket);
		if(network_send(pClientConn->nSocket, (char *)&sendPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
		{
			m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send response message, close the connection! %s:%d\n", __FILE__, __LINE__);
			CloseClient(pClientConn);
		}
		break;
	case C2L_CMD_UNREGISTER: //Client�ڵ�ע��
		sClientAppType = recvPacket.cBuffer;
		m_pLogger->Write(CRunLogger::LOG_WARNING, "ClientNode(ID=%d, AppType=%s, IP=%s) logout, maybe exited humanlly\n", \
			nClientID, sClientAppType.c_str(), pClientConn->sIP.c_str());
		CloseClient(pClientConn);
		break;
	case C2L_CMD_CLOSE: //Client�ڵ���������ر�����
		sClientAppType = recvPacket.cBuffer;
		CloseClient(pClientConn);
		break;
	default: //û�з��������Ϣ����
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid message command(%d) received, close the connection(%s:%d)! %s:%d\n",
			recvPacket.nCommand, pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
		CloseClient(pClientConn);
		break;
	}
}

bool CMasterListener::GetClientAppUpdateVersion(const string& sAppType, int nCurAppVersion, _stAppVerInfo &dAppVerInfo)
{
	bool ret = false;
	LOCK(m_pConfigure->locker4ClientApp);
	map<string, CAppUpdateInfo*>::iterator it = m_pConfigure->oClientAppVersion.find(sAppType);
	if(it != m_pConfigure->oClientAppVersion.end())
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
	UNLOCK(m_pConfigure->locker4ClientApp);
	return ret;
}


bool CMasterListener::GetTaskInfo(int nClientID, const string& sAppType, _stTaskReqInfo &task)
{
	string sTaskZipFilePath = "";
	assert(m_pTaskManager != NULL);
	int nTaskID = 0;
	if(m_pTaskManager->RequestTask(nClientID, sAppType, sTaskZipFilePath, nTaskID)) //ȡ��һ������
	{
		task.nTaskID = nTaskID;
		task.usPort = m_pConfigure->nTaskPort;
		strcpy(task.chTaskFile, sTaskZipFilePath.c_str());
		return true;
	}
	return false;
}

void CMasterListener::CloseClient(TClientConn* pClientConn)
{
	assert(pClientConn != NULL);

	//��������Ϊ����������˵��Client�ڵ�Ͽ��ˣ���ɾ����Client�ڵ�
	if(pClientConn->nConnType == LONG_CONN_HEART)
		m_pClientManager->RemoveClient(pClientConn->nClientID);
	
	FD_CLR(pClientConn->nSocket, &m_fdAllSet);
	CLOSE_SOCKET(pClientConn->nSocket);
	pClientConn->nSocket = INVALID_SOCKET;
}
