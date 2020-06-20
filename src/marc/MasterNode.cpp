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
	//初始化Master监听socket
	m_nListenSocket = network_listener_init(m_pConfigure->sIP.c_str(), m_pConfigure->nPort, MARC_MAX_CONN_COUNT);
	if(m_nListenSocket == INVALID_SOCKET)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "MasterNode init failed! Please check IP(%s) and Port(%d)! %s:%d\n",
			m_pConfigure->sIP.c_str(), m_pConfigure->nPort, __FILE__, __LINE__);
		return false;
	}
	m_nMaxSocket = m_nListenSocket;

	//fdset初始化
	FD_ZERO(&m_fdAllSet);
	FD_SET(m_nListenSocket, &m_fdAllSet);

	//初始化任务下载服务并启动之
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

		//失败则换一个端口
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

	//初始化监听服务组并启动之
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

	//启动Master服务线程
	if(!m_pMasterThread->Start(0))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to start thread! %s:%d\n", __FILE__, __LINE__);
		return false;
	}

	//启动任务调度线程
	if(!m_pTaskThread->Start())
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to start thread! %s:%d\n", __FILE__, __LINE__);
		return false;
	}

	//启动状态保存线程
	if(!m_pStateSaveThread->Start())
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to start thread! %s:%d\n", __FILE__, __LINE__);
		return false;
	}

	//启动节点资源监控线程
	if(!m_pWatchThread->Start())
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to start thread! %s:%d\n", __FILE__, __LINE__);
		return false;
	}
	
	return true;
}

void CMasterNode::Stop()
{
	//停止任务调度
	assert(m_pTaskThread != NULL);
	m_pTaskThread->Stop();

	//停止Master服务线程
	assert(m_pMasterThread != NULL);
	m_pMasterThread->Stop();

	//停止各个监听服务
	for(size_t i = 0; i < m_oListeners.size(); i++)
	{
		CMasterListener* pListener = m_oListeners[i];
		assert(pListener != NULL);
		pListener->Stop();
		delete pListener;
	}
	m_oListeners.clear();

	//停止任务下载服务
	if(m_pTaskSvr != NULL)
	{
		sftp_server_stop(m_pTaskSvr);
		sftp_server_exit(m_pTaskSvr);
		m_pTaskSvr = NULL;
	}

	//关闭Master与各个客户端连接
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

	//关闭Master监听套接字
	FD_CLR(m_nListenSocket, &m_fdAllSet);
	CLOSE_SOCKET(m_nListenSocket);
	m_nListenSocket = INVALID_SOCKET;
	m_nMaxSocket = INVALID_SOCKET;

	//停止资源监控
	assert(m_pWatchThread != NULL);
	m_pWatchThread->Stop();

	//停止状态保存
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

	//删除已关闭的连接以及超时的短连接
	list<TClientConn*>::iterator it = me->m_oClientConns.begin();
	for(; it != me->m_oClientConns.end(); )
	{
		TClientConn* pClientConn = (*it);
		assert(pClientConn != NULL);

		if(pClientConn->nSocket != INVALID_SOCKET)
		{
			//检测连接是否存活超时
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

			//超时则关闭连接
			if(bConnTimeout)
			{
				me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Connection (type=%d, %s:%d) timeout detected! close it! %s:%d\n", 
					pClientConn->nConnType, pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
				me->CloseClient(pClientConn);
			}
		}

		//删除已关闭的客户端
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

	//套接字队列初始化
	fd_set fdRead, fdException;
	memcpy(&fdRead, &me->m_fdAllSet, sizeof(fd_set));
	memcpy(&fdException, &me->m_fdAllSet, sizeof(fd_set));

	//select操作
	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;
	int inReady = select((int)me->m_nMaxSocket+1, &fdRead, NULL, &fdException, &timeout);
	if(inReady < 0)//错误
	{
		me->m_pLogger->Write(CRunLogger::LOG_ERROR, "select error: %s! %s:%d\n", strerror(errno), __FILE__, __LINE__);
		Sleep(2000);
		return ; 
	}
	if(inReady == 0)//超时
	{
		//me->m_pLogger->Write(CRunLogger::LOG_INFO, "select timeout, no message need to be processed\n");
		return; 
	}

	if(FD_ISSET(me->m_nListenSocket, &fdRead))
	{
		//新客户端连接
		sockaddr_in addrRemote;
		socklen_t nAddrLen = sizeof(sockaddr_in);
		SOCKET nClientSock = accept(me->m_nListenSocket,(sockaddr*)&addrRemote,&nAddrLen);
		if(nClientSock != INVALID_SOCKET)
		{
			FD_SET(nClientSock, &me->m_fdAllSet);

			//创建连接对象加入连接对列
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

			//记录最大套接字
			if(me->m_nMaxSocket < nClientSock)
				me->m_nMaxSocket = nClientSock;
		}
		else
		{
			me->m_pLogger->Write(CRunLogger::LOG_ERROR, "accept failed! %s. %s:%d\n", strerror(errno), __FILE__, __LINE__);
		}

		if(--inReady<=0) return ;
	}

	//接收并处理各个客户端的消息
	it = me->m_oClientConns.begin();
	for(; it!=me->m_oClientConns.end() && inReady > 0; ++it)
	{
		TClientConn *pClientConn = (*it);
		assert(pClientConn->nSocket != INVALID_SOCKET);
		if(FD_ISSET(pClientConn->nSocket, &fdException)) //异常
		{
			me->m_pLogger->Write(CRunLogger::LOG_WARNING, "socket exception detected, maybe connection(%s:%d) closed! %s:%d\n",
				pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
			pClientConn->nLastActiveTime = (int)time(0);
			me->CloseClient(pClientConn);
			--inReady;
		}
		else if(FD_ISSET(pClientConn->nSocket, &fdRead)) //有消息来
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
	_stDataPacket sendPacket; //返回给客户端的消息包
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
	case C2M_CMD_REGISTER_REQ:	//Client节点注册，返回实际的监听IP和端口
		pClientConn->nConnType = SHORT_CONN_DEFAULT;
		ParseAppTypeAndInstallPath(recvPacket.cBuffer, sAppType, sInstallPath);
		m_pLogger->Write(CRunLogger::LOG_INFO, "ClientNode(ID=%d, AppType=%s, IP=%s) request register\n", \
			nClientID, sAppType.c_str(), pClientConn->sIP.c_str());
		if(recvPacket.nOffset == MARC_MAGIC_NUMBER) //魔数合法性检查
		{
			//ClientID为0时为其分配一个ID
			if(nClientID == 0)
			{
				nClientID = m_nClientIDBase++;
				sendPacket.nClientID = nClientID;
				m_pLogger->Write(CRunLogger::LOG_INFO, "Assign ID=%d to the ClientNode\n", sendPacket.nClientID);
			}

			// 若该节点ID不存在或虽存在但该节点已失效则将负载最小的监听服务告知Client节点
			if(!m_pClientManager->FindClient(nClientID, bDisabled) || bDisabled)
			{
				pClientConn->nClientID = nClientID;

				//获得负载最小的Listener
				CMasterListener* pListener = SelectListener();
				assert(pListener != NULL);
				sendPacket.nCommand = M2C_CMD_REGISTER_YES;
				sendPacket.nOffset = pListener->GetListenPort();
				m_pLogger->Write(CRunLogger::LOG_INFO, "Assign Listener(Port=%d) to the ClientNode(ID=%d)\n", sendPacket.nOffset, nClientID);
			}
			else //该Client节点ID已存在则不允许注册
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
	case R2M_CMD_HEART_SEND: //Result节点发来心跳
		pClientConn->nConnType = LONG_CONN_HEART;
		pClientConn->nClientID = nClientID;
		pResultStatus = (_stResultStatus*)recvPacket.cBuffer;
		Net2Host(*pResultStatus);
		
		//记录该Result节点的状态和最近活跃时间
		m_pResultNodeManager->SetRunningStatus(nClientID, pResultStatus);
		m_pResultNodeManager->SetActiveTime(nClientID, time(0));

		//m_pLogger->Write(CRunLogger::LOG_INFO, "Heartbeat from ResultNode(ID=%d, %s:%d, overload=%d) received...\n",
		//	nClientID, pClientConn->sIP.c_str(), pClientConn->nPort, pResultStatus->nOverload);
		break;
	case R2M_CMD_SOURCE_STATUS: //节点资源使用状况
		pClientConn->nConnType = LONG_CONN_HEART;

		//记录该Result节点的资源使用状况
		pSourceStatus = (_stNodeSourceStatus*)recvPacket.cBuffer;
		Net2Host(*pSourceStatus);
		m_pResultNodeManager->SetSourceStatus(nClientID, pSourceStatus);
		break;
	case R2M_CMD_APPVER_REQ: //Result节点请求App版本更新
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
	case R2M_CMD_ERRLOG_INFO: //Result节点发来异常日志信息
		m_pResultNodeManager->AddErrLog(nClientID, recvPacket.cBuffer);
		break;
	case R2M_CMD_REGISTER_REQ: //Result节点请求注册，返回应答消息
		pClientConn->nConnType = LONG_CONN_HEART;
		pResultNode = (_stResultNode *)recvPacket.cBuffer;
		Net2Host(*pResultNode);
		m_pLogger->Write(CRunLogger::LOG_INFO, "ResultNode(AppType=%s, %s:%d|%s) request register\n",
			pResultNode->chAppType, pResultNode->chIp, pResultNode->iPort, pResultNode->chSavePath);
		if(recvPacket.nOffset == MARC_MAGIC_NUMBER) //魔数合法性检查
		{
			//判断Result节点是否已存在
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
			else if(bDisabled) //找到相同的Result节点但其已失效则恢复该节点
			{
				sendPacket.nCommand = M2R_CMD_REGISTER_YES;
				sendPacket.nClientID = nResultID;
				m_pResultNodeManager->AddResultNode(sendPacket.nClientID, pResultNode);
				pClientConn->nClientID = sendPacket.nClientID;
				pClientConn->nPort = pResultNode->iPort;
				m_pLogger->Write(CRunLogger::LOG_INFO, "ResultNode(AppType=%s, %s:%d|%s) resume register, ID=%d\n",
					pResultNode->chAppType, pResultNode->chIp, pResultNode->iPort, pResultNode->chSavePath, sendPacket.nClientID);
			}
			else //找到相同的Result节点
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

		//回复Result节点
		Host2Net(sendPacket);
		if(network_send(pClientConn->nSocket,(char *)&sendPacket,sizeof(_stDataPacket)) != MARC_NETWORK_OK)
		{
			m_pLogger->Write(CRunLogger::LOG_ERROR, "network_send failed, close the connection(%s:%d)! %s:%d\n", 
				pClientConn->sIP.c_str(), pClientConn->nPort, __FILE__, __LINE__);
			CloseClient(pClientConn);
		}
		break;
	case R2M_CMD_INSTALL_PATH: //Result节点告知部署路径
		sInstallPath = recvPacket.cBuffer;
		m_pResultNodeManager->SetInstallPath(nClientID, sInstallPath.c_str());
		m_pLogger->Write(CRunLogger::LOG_INFO, "ResultNode(ID=%d, %s:%d) install-path is %s ...\n",
			nClientID, pClientConn->sIP.c_str(), pClientConn->nPort, sInstallPath.c_str());
		break;
	case R2M_CMD_UNREGISTER: //Result节点注销
		m_pLogger->Write(CRunLogger::LOG_WARNING, "ResultNode(ID=%d, %s:%d) logout, maybe exited humanlly\n", 
			pClientConn->nClientID, pClientConn->sIP.c_str(), pClientConn->nPort);
		CloseClient(pClientConn);
		break;
	case R2M_CMD_CLOSE: //连接关闭
		CloseClient(pClientConn);
		break;
	case C2M_CMD_CLOSE: //连接关闭
		CloseClient(pClientConn);
		break;
	default: //其他消息
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
	//若该连接为心跳连接则说明Result节点断开了，需删除该Result节点
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
	//本线程函数用于生成任务
	CMasterNode* me = (CMasterNode*)param;

	//取出需创建任务的Client节点的ID及其应用程序类型
	vector<int> oFreeClientIDs;
	vector<string> oFreeClientAppTypes;
	me->m_pClientManager->GetClientsOfNeedCreateTask(oFreeClientIDs, oFreeClientAppTypes);
	if(oFreeClientIDs.empty())
	{
		Sleep(1000);
		return ;
	}

	//为每个没有待处理任务的Client节点生成任务
	assert(oFreeClientIDs.size() == oFreeClientAppTypes.size());
	for(size_t i=0; i < oFreeClientIDs.size(); i++)
	{
		int nClientID = oFreeClientIDs[i];
		const string& sClientAppType = oFreeClientAppTypes[i];

		//执行任务生成程序（需根据各个节点的上次任务创建失败时间判断本次是否要为其创建任务）
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

	//清空任务文件夹
	CleanDir(me->m_pConfigure->sTaskPath.c_str());
}

bool CMasterNode::ExecTaskCreateApp(int nClientID, const string& sAppType)
{
	time_t t1 = time(0);

	//获得任务生成命令
	string sAppCmd = "";
	if(!GetAppCmd(sAppType, sAppCmd)) return false;
	if(sAppCmd.empty()) return false;

    //创建任务存放文件夹路径
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

	//运行任务生成程序（以阻塞方式，设置超时），接口: [AppCmd] [TaskDirPath] [ClientID] 
	//如"./NewsTaskCreate ./task/1_NewsGather_task_20100204095923/ 1"
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

	//检查是否执行成功（执行成功的须在sTaskDirPath文件夹中创建.success标志文件）
	char sAppFlagFile[1024] = {0};
	sprintf(sAppFlagFile, "%s.success", sTaskDirPath);
	if(!DIR_EXIST(sAppFlagFile))
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "Flag file '%s' not found, appliation program '%s' exited abnormally! %s:%d\n", 
			sAppFlagFile, sAppCmd.c_str(), __FILE__, __LINE__);
		deleteDir(sTaskDirPath);
		return false;
	}

	//删除.success标志文件，检查文件夹是否空
	deleteFile(sAppFlagFile);
	CDirScanner ds(sTaskDirPath, false);
	const vector<string>& oFileList = ds.GetAllList();
	if(oFileList.empty())
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "No data file found in folder %s! %s:%d\n",
			sTaskDirPath, __FILE__, __LINE__);
		return false;
	}

	//压缩任务文件夹
	string sTaskZipFilePath = "";
	if(!MyZipTask(chTaskDirName, sTaskZipFilePath))
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "myzip failed: %s, %s:%d\n", chTaskDirName, __FILE__, __LINE__);
		deleteDir(sTaskDirPath);
		return false;
	}

	//检查压缩文件是否存在
	if(!DIR_EXIST(sTaskZipFilePath.c_str()))
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "task-zipfile not found: %s, maybe myzip failed. [%s:%d]\n", sTaskZipFilePath.c_str(), __FILE__, __LINE__);
		return false;
	}

	//加入待处理任务对列
	time_t t2 = time(0);
	if(!m_pTaskManager->AddTask(nClientID, sAppType, sTaskZipFilePath, t2-t1)) 
	{
		//无法加入到任务对列
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
	//先查询内存中是否已有
	map<string,string>::const_iterator it = m_pConfigure->oAppType2AppCmd.find(sAppType);
	if(it != m_pConfigure->oAppType2AppCmd.end())
	{
		sAppCmd = it->second;
		return true;
	}

	//从配置文件中读取
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
	//每隔nMaxSaveStateTime秒保存一次
	if(me->m_oListeners.empty()) return ;
	if(time(0) - me->m_nLastSaveTime < me->m_pConfigure->nMaxSaveStateTime) return ; 
	me->m_nLastSaveTime = (int)time(0);

	//执行各个Listener的状态保存函数
	string sCurTime = formatDateTime(time(0), 1);
	vector<string> oStateFiles;
	me->m_pClientManager->SaveClientState(sCurTime, oStateFiles);

	if(oStateFiles.empty())
	{
		return ;
	}

	//将各个状态文件名写入.list文件
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

	//防止读写冲突,需要写一个结束标记
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
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/love.png\"><b>&nbsp;主机地址: </b>%s:%d，<b>节点数目: </b>Result节点 %d个，Client节点 %d个</p></font>\n",m_pConfigure->sIP.c_str(),m_pConfigure->nPort,m_pResultNodeManager->NodeCount(), m_pClientManager->NodeCount()); html += buf;
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/home.png\"><b>&nbsp;部署位置: </b>%s\n</p></font>", m_pConfigure->sInstallPath.c_str()); html += buf;
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/clock.png\"><b>&nbsp;启动时间: </b>%s 【已运行 %d 天 %d 小时 %d 分钟】</p></font>\n", formatDateTime(m_pConfigure->nStartupTime).c_str(), nRunDays, nRunHours, nRunMinutes); html += buf;
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/wifi.png\"><b>&nbsp;活跃连接数: </b>当前总共%d个活跃连接，其中%d个任务下载连接</p></font>\n", GetActiveConns(), sftp_server_active_conns(m_pTaskSvr)); html += buf;
	
	LOCK(m_pTaskStatInfo->m_locker);
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/info.png\"><b>&nbsp;创建任务数: </b>%d个, 平均创建时间: %d秒</p></font>\n", m_pTaskStatInfo->nTotalCreatedTasks, m_pTaskStatInfo->nTotalCreatedTasks==0?0:m_pTaskStatInfo->nTotalCreatedTimeUsed/m_pTaskStatInfo->nTotalCreatedTasks); html += buf;
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/task.png\"><b>&nbsp;最近创建任务: </b>ID=%d, 任务文件为%s</p></font>\n", m_pTaskStatInfo->nLastCreatedTaskID, m_pTaskStatInfo->sLastCreatedTaskFile.c_str()); html += buf;
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/wifi.png\"><b>&nbsp;任务执行情况: </b>分发任务数: %d个, 完成任务数: %d个, 失败任务数: %d个</p></font>\n", m_pTaskStatInfo->nTotalDeliveredTasks, m_pTaskStatInfo->nTotalFinishdTasks, m_pTaskStatInfo->nTotalFailedTasks); html += buf;
	UNLOCK(m_pTaskStatInfo->m_locker);
	
	LOCK(m_locker);
	_snprintf(buf, sizeof(buf), "<font size=4><p><img border=0 src=\"./img/activity.png\"><b>&nbsp;资源使用情况: </b>CPU 空闲率: %d%%, 磁盘剩余率 %d%%, 内存空闲率 %d%%, 网卡速率: %d bps (获取时间: %s)</p></font>\n", m_dSourceStatus.cpu_idle_ratio, m_dSourceStatus.disk_avail_ratio, m_dSourceStatus.memory_avail_ratio, m_dSourceStatus.nic_bps, formatDateTime(m_dSourceStatus.watch_timestamp).c_str()); html += buf;
	UNLOCK(m_locker);

	html += "</body></html>";
}
