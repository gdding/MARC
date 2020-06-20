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

	//初始化m_pAppRunner
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

	//Master节点的IP和主监听端口
	m_sMasterIP = bUseBakMaster?m_pConfigure->sBakMasterIP:m_pConfigure->sMasterIP;
	m_nMasterPort = bUseBakMaster?m_pConfigure->nBakMasterPort:m_pConfigure->nMasterPort;

	//载入上次未上传的结果压缩文件
	if(m_pConfigure->nResultUploadEnabled != 0)
	{
		m_pLogger->Write(CRunLogger::LOG_INFO, "Load unfinished result-zipfile...\n");
		LoadUploadResultFile();
	}

	//注册到Master节点	
	int nRetCode = RegisterToMaster();
	if(nRetCode != MARC_CODE_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't register to MasterNode!\n");
		m_nLastErrCode = nRetCode;
		m_bNeedRestart = true;
		return false;
	}
	m_pLogger->Write(CRunLogger::LOG_INFO, "Register to MasterNode successfully!\n");

	//显示应用程序版本号并检查是否需要进行升级
	if(m_pConfigure->nUpdateEnabled != 0)
	{
		m_pLogger->Write(CRunLogger::LOG_INFO, "Current application version is %d\n", m_pConfigure->nCurAppVersion);
		_stAppVerInfo dAppVerInfo;
		if(CheckAppVersion(dAppVerInfo))
			UpdateAppVersion(dAppVerInfo);
		m_nLastAppVerCheckTime = time(0);
	}

	//启动心跳线程
	if(!m_pHeartThread->Start())
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't start thread! %s:%d\n", __FILE__, __LINE__);
		UnregisterToMaster();
		m_bNeedRestart = true;
		return false;
	}

	//启动任务处理线程
	if(!m_pTaskThread->Start())
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't start thread! %s:%d\n", __FILE__, __LINE__);
		UnregisterToMaster();
		m_bNeedRestart = true;
		return false;
	}

	//启动异步上传线程
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
	//停止异步上传线程
	if(m_pConfigure->nAsynUpload == 1)
	{
		m_pAsynUploadThread->Stop();
	}

	//停止心跳线程和任务处理线程
	m_pHeartThread->Stop();
	m_pTaskThread->Stop();

	//取消注册
	UnregisterToMaster();

	//若节点程序正在运行则kill掉
	if(m_pAppRunner->IsAppRunning())
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "Application still running, kill it and cleanup input and output folder! %s:%d\n", __FILE__, __LINE__);
		KillApp();
		CleanDir(m_pConfigure->sInputDir.c_str());
		CleanDir(m_pConfigure->sOutputDir.c_str());
	}

	//保存仍未上传的结果文件名
	m_pLogger->Write(CRunLogger::LOG_INFO, "Save unfinished result-zipfile...\n");
	SaveUploadResultFiles();
}

void CClientNode::AsynUploadRutine(void *param)
{
	/*****
	* 从待上传文件对列中取出一个文件上传到Result节点
	* 若上传失败则将该文件加入对列尾部等待以后再上传
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

//Client节点状态监控线程，定期心跳并将节点状态发送给Master节点
void CClientNode::HeartRutine(void* param)
{
	CClientNode* me = (CClientNode*)param;
	if(me->m_nHeartSocket == INVALID_SOCKET) return ;

	//fdset初始化
	fd_set fdRead, fdWrite, fdException;
	memcpy(&fdRead, &me->m_fdAllSet, sizeof(fd_set));
	memcpy(&fdWrite, &me->m_fdAllSet, sizeof(fd_set));
	memcpy(&fdException, &me->m_fdAllSet, sizeof(fd_set));

	//select操作
	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;
	int inReady = select((int)me->m_nHeartSocket+1, &fdRead, &fdWrite, &fdException, &timeout);
	if(inReady == 0) //超时
	{
		Sleep(1000);
		return ; 
	}
	if(inReady < 0)  //错误
	{
		me->m_pLogger->Write(CRunLogger::LOG_ERROR, "select error: %s! close SOCKET! %s:%d\n", strerror(errno), __FILE__, __LINE__);
		CLOSE_SOCKET(me->m_nHeartSocket);
		me->m_nHeartSocket = INVALID_SOCKET;
		me->m_nLastErrCode = MARC_CODE_SELECT_FAILED;
		me->m_bNeedRestart = true;
		return ; 
	}
	if(FD_ISSET(me->m_nHeartSocket, &fdException)) //异常
	{
		CLOSE_SOCKET(me->m_nHeartSocket);
		me->m_nHeartSocket = INVALID_SOCKET;
		me->m_nLastErrCode = MARC_CODE_SOCKET_EXCEPTION;
		me->m_pLogger->Write(CRunLogger::LOG_ERROR, "select exception, maybe closed by MasterNode! %s:%d\n", __FILE__, __LINE__);
		me->m_bNeedRestart = true;
		return ; 
	}
	if(FD_ISSET(me->m_nHeartSocket, &fdRead)) //Master节点有消息发来
	{
		_stDataPacket recvPacket; //接收到的消息包
		memset(&recvPacket, 0, sizeof(_stDataPacket));
		int nRecev = network_recv(me->m_nHeartSocket, (char*)&recvPacket, sizeof(_stDataPacket));
		switch(nRecev)
		{
		case MARC_NETWORK_OK:			
			//处理Master发来的消息
			Net2Host(recvPacket);
			me->HandleMasterMsg(recvPacket);
			break;
		case MARC_NETWORK_TIMEOUT: //超时
			me->m_pLogger->Write(CRunLogger::LOG_WARNING, "network_recv timeout! %s:%d\n", __FILE__, __LINE__);
			break;
		case MARC_NETWORK_CLOSED: //连接被关闭
			CLOSE_SOCKET(me->m_nHeartSocket);
			me->m_nHeartSocket = INVALID_SOCKET;
			me->m_nLastErrCode = MARC_CODE_SOCKET_EXCEPTION;
			me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Heartbeat connection closed by MasterNode! %s:%d\n", __FILE__, __LINE__);
			me->m_bNeedRestart = true;
			return ;
		case MARC_NETWORK_ERROR: //网络错误
			CLOSE_SOCKET(me->m_nHeartSocket);
			me->m_nHeartSocket = INVALID_SOCKET;
			me->m_nLastErrCode = MARC_CODE_SOCKET_EXCEPTION;
			me->m_pLogger->Write(CRunLogger::LOG_ERROR, "Network error: %s! %s:%d\n", strerror(errno), __FILE__, __LINE__);
			me->m_bNeedRestart = true;
			return ;
		default: //应该不会到这
			assert(false);
			break;
		};
	}
	if(FD_ISSET(me->m_nHeartSocket, &fdWrite))
	{
		//发送心跳信息
		if(time(0) - me->m_nLastHeartTime >= me->m_pConfigure->nHeartbeatInterval)
		{
			me->m_nLastHeartTime = (int)time(0);

			//取得当前节点状态
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
			case MARC_NETWORK_OK: //发送成功
				me->m_pLogger->Write(CRunLogger::LOG_INFO, "Send heartbeat to MasterNode successfully\n");
				break;
			default: //发送失败
				//CLOSE_SOCKET(me->m_nHeartSocket);
				//me->m_nHeartSocket = INVALID_SOCKET;
				//me->m_nLastErrCode = MARC_CODE_SEND_FAILED;
				me->m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to send heartbeat to MasterNode! %s:%d\n", __FILE__, __LINE__);
				//me->m_bNeedRestart = true;
				break;
			}
		}

#ifndef WIN32
		//发送节点资源使用状况信息
		if(time(0) - me->m_nLastSourceStatusSendTime >= me->m_pConfigure->nSourceStatusInterval)
		{
			me->m_nLastSourceStatusSendTime = (int)time(0);

			//取得当前节点资源使用状况
			_stNodeSourceStatus status;
			if(!GetSourceStatusInfo(status))
			{
				me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Can't acquire source status!\n");
			}
			else
			{
				//发送给Master
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
				case MARC_NETWORK_OK: //发送成功
					me->m_pLogger->Write(CRunLogger::LOG_INFO, "Send source status to MasterNode successfully\n");
					break;
				default: //发送失败
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
		//发送状态信息(dgd:这里不宜用心跳连接来发送)
		if(time(0) - me->m_nLastAppStateSendTime > me->m_pConfigure->nStateInterval)
		{
			me->m_nLastAppStateSendTime = (int)time(0);
			me->m_pLogger->Write(CRunLogger::LOG_INFO, "Send application status to MasterNode...\n");

			//从状态文件读取状态信息，传送给Master节点
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

	//检查App版本号若有更新则进行版本升级
	if(me->m_pConfigure->nUpdateEnabled != 0 && time(0) - me->m_nLastAppVerCheckTime > me->m_pConfigure->nUpdateInterval)
	{
		me->m_nLastAppVerCheckTime = time(0);
		_stAppVerInfo dAppVerInfo;
		if(me->CheckAppVersion(dAppVerInfo))
			me->UpdateAppVersion(dAppVerInfo);
	}

	//节点程序未启动则意味着节点在等待新任务
	if(!me->m_bAppStarted)
	{
		me->OnAppNeedTask();
		return ;
	}

	//节点程序已启动
	assert(me->m_bAppStarted);

	//判断节点程序是否运行结束
	if(me->m_pAppRunner->IsAppRunning()) //正在运行
	{
		//判断是否运行超时,超时则kill掉
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

	//判断程序是否执行成功（执行成功的程序应在当前目录下生成.success文件）
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

		//清空input output文件夹
		CleanDir(me->m_pConfigure->sInputDir.c_str());
		CleanDir(me->m_pConfigure->sOutputDir.c_str());
		me->m_nAppStartTime = 0;
		me->m_bAppStarted  = false;
	}
	else //非正常结束
	{
		me->m_pLogger->Write(CRunLogger::LOG_WARNING, "Application running finished abnormally (.success not found)! %s:%d\n", __FILE__, __LINE__);
		me->OnAppFailed();
	}
	
	Sleep(1000);
	return ;
}

bool CClientNode::CheckAppVersion(_stAppVerInfo &dAppVerInfo)
{
	//连接到Master节点监听服务
	m_pLogger->Write(CRunLogger::LOG_INFO, "Request to MasterNode for Application version update...\n");
	SOCKET nSocket = network_connect(m_sMasterIP.c_str(), m_nListenerPort);
	if(nSocket == INVALID_SOCKET) 
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't connect to MasterNode! %s:%d\n", __FILE__, __LINE__);
		return false;
	}

	//向Master节点发送APP版本更新请求
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

	//检查响应消息
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

	//关闭与Master节点监听服务的连接
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
	//下载升级包
	m_pLogger->Write(CRunLogger::LOG_INFO, "Download update-zipfile from MasterNode (file path: %s) ...\n", dAppVerInfo.chUpdateFile);
	time_t nStartTime = time(0);
	string sLocalZipFile = m_pConfigure->sZipUpdateDir + getFileName(dAppVerInfo.chUpdateFile, true);
	int nRetCode = sftp_client_download_file(m_sMasterIP.c_str(), dAppVerInfo.usPort, dAppVerInfo.chUpdateFile, sLocalZipFile.c_str());
	unsigned int nTimeUsed = (unsigned int)(time(0) - nStartTime);
	switch(nRetCode)
	{
	case SFTP_CODE_OK: //下载成功
		m_pLogger->Write(CRunLogger::LOG_INFO, "update-zipfile downloaded successfully (version=%d, file=%s), %d seconds\n", 
			dAppVerInfo.nUpdateVersion, sLocalZipFile.c_str(), nTimeUsed);
		break;
	default: //其他错误
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to download update-zipfile(version=%d, File=%s), ErrCode: %d, %s:%d\n",
			dAppVerInfo.nUpdateVersion, sLocalZipFile.c_str(), nRetCode, __FILE__, __LINE__);
		return ;
	}

	//解压升级包
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

	//保存当前版本号
	m_pConfigure->nCurAppVersion = dAppVerInfo.nUpdateVersion;
	WriteAppVersion(m_pConfigure->sAppVerFile, m_pConfigure->sAppType, m_pConfigure->nCurAppVersion);
	m_pLogger->Write(CRunLogger::LOG_INFO, "Application updated successfully! New version is %d\n", m_pConfigure->nCurAppVersion);
}

void CClientNode::OnAppFailed()
{
	//清空input和output文件夹
	m_pLogger->Write(CRunLogger::LOG_INFO, "Application running failed, cleanup input and output folder\n");
	CleanDir(m_pConfigure->sInputDir.c_str());
	CleanDir(m_pConfigure->sOutputDir.c_str());

	//发送消息告知Master端
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

	//记录状态
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

	//发送消息告知Master端
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

	//获得Master节点的从监听端口
	m_pLogger->Write(CRunLogger::LOG_INFO, "Accquire listener port from MasterNode...\n");
	nRetCode = GetListenerPort(m_nListenerPort);
	if(nRetCode != MARC_CODE_OK)
	{
		return nRetCode;
	}	

	//连接Master节点的从监听服务
	//备注：请求注册用的SOCKET将一直保持，用于心跳发送和状态信息发送
	m_pLogger->Write(CRunLogger::LOG_INFO, "Connect to Listener(%s:%d) of MasterNode...\n", m_sMasterIP.c_str(), m_nListenerPort);
	m_nHeartSocket = network_connect(m_sMasterIP.c_str(), m_nListenerPort);
	if(m_nHeartSocket == INVALID_SOCKET)
	{
		m_pLogger->Write(CRunLogger::LOG_INFO, "Can't connect to listener of MasterNode! %s:%d\n", __FILE__, __LINE__);
		return MARC_CODE_CONNECT_FAILED;
	}

	//向监听器发送注册请求消息（Client节点必须带有标示MARC_MAGIC_NUMBER,Master才认）
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

	//接收注册应答消息
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	if(network_recv(m_nHeartSocket, (char*)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		CLOSE_SOCKET(m_nHeartSocket);
		m_nHeartSocket = INVALID_SOCKET;
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to receive response message of C2L_CMD_REGISTER_REQ from MasterNode! %s:%d\n", __FILE__, __LINE__);
		return MARC_CODE_RECV_FAILED;
	}
	Net2Host(msgPacket);

	//根据不同的注册命令结果判断
	switch(msgPacket.nCommand)
	{
	case L2C_CMD_REGISTER_YES: //注册成功
		break;
	case L2C_CMD_REGISTER_NO: //注册失败
		CLOSE_SOCKET(m_nHeartSocket);
		m_nHeartSocket = INVALID_SOCKET;
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Refused to register to MasterNode! %s:%d\n", __FILE__, __LINE__);
		return MARC_CODE_REGISTER_REFUSED;
	case L2C_CMD_INVALID_CLIENT: //非法Client节点
		CLOSE_SOCKET(m_nHeartSocket);
		m_nHeartSocket = INVALID_SOCKET;
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid ClientNode detected by MasterNode! %s:%d\n", __FILE__, __LINE__);
		return MARC_CODE_REGISTER_REFUSED;
	default: //其他命令不合法
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
		//断开与Master节点的连接
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
	//目前Master节点不会通过心跳连接给Client节点发消息
	m_pLogger->Write(CRunLogger::LOG_INFO, "Receive message(command:%d) from MasterNode.\n", msgPacket.nCommand);
}

int CClientNode::GetListenerPort(unsigned short& nListenerPort)
{
	//连接Master节点
	SOCKET nClientSock = network_connect(m_sMasterIP.c_str(), m_nMasterPort);
	if(nClientSock == INVALID_SOCKET)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't connect to MasterNode(%s:%d)! %s:%d\n", m_sMasterIP.c_str(), m_nMasterPort, __FILE__, __LINE__);
		return MARC_CODE_CONNECT_FAILED;
	}

	//发送注册请求消息(必须带有标示MARC_MAGIC_NUMBER, Master端才认)
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

	//接收并处理注册应答消息
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
	case M2C_CMD_REGISTER_YES://注册成功,得到实际的从监听服务的IP和端口
		nListenerPort = msgPacket.nOffset;

		if(msgPacket.nClientID > 0)
		{
			assert(m_pConfigure->nClientID == 0);
			m_pConfigure->nClientID = msgPacket.nClientID;
			m_pLogger->Write(CRunLogger::LOG_INFO, "Assigned ClientID=%d\n", m_pConfigure->nClientID);
		}
		break;
	case M2C_CMD_REGISTER_NO: //注册失败
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Refused to register to MasterNode! %s:%d\n", __FILE__, __LINE__);
		nRetCode = MARC_CODE_REGISTER_REFUSED;
		break;
	default: //无效命令
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid message command(%d) received from MasterNode! %s:%d\n", msgPacket.nCommand, __FILE__, __LINE__);
		nRetCode = MARC_CODE_INVALID_COMMAND;
		break;
	};

	//关闭与Master节点监听服务的连接
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
	//连接Master监听服务
	SOCKET nSocket = network_connect(m_sMasterIP.c_str(), m_nListenerPort);
	if(nSocket == INVALID_SOCKET) 
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't connect to MasterNode! %s:%d\n", __FILE__, __LINE__);
		m_bNeedRestart = true;
		return MARC_CODE_CONNECT_FAILED;
	}

	//发送任务请求消息
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

	//接收任务请求应答消息
	_stDataPacket stRecvPack;
	memset(&stRecvPack, 0, sizeof(_stDataPacket));
	if(network_recv(nSocket, (char*)&stRecvPack, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to receive response message of C2L_CMD_TASK_REQ from MasterNode! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return MARC_CODE_RECV_FAILED;
	}
	Net2Host(stRecvPack);

	//根据应答命令进行相应处理
	int nRetCode = MARC_CODE_OK;
	bool bNeedRestart = false;
	switch(stRecvPack.nCommand)
	{
	case L2C_CMD_TASK_YES: //有任务
		memcpy(&task, stRecvPack.cBuffer, sizeof(_stTaskReqInfo));
		Net2Host(task);
		nRetCode = MARC_CODE_OK;
		break;
	case L2C_CMD_TASK_NO: //无任务（也可能是Result节点没启动）
		nRetCode = MARC_CODE_NOTASK;
		break;
	case L2C_CMD_INVALID_CLIENT: //Client无效
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid ClientNode detected by MasterNode, maybe deleted from node queue by MasterNode for network exception! %s:%d\n", __FILE__, __LINE__);
		nRetCode = MARC_CODE_INVALID_CLIENT;
		bNeedRestart = true;
		break;
	default: //非法命令
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid message command(%d) received from MasterNode! %s:%d\n", stRecvPack.nCommand, __FILE__, __LINE__);
		nRetCode = MARC_CODE_INVALID_COMMAND;
		break;
	};

	//断开本次连接
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
	//连接Master监听服务
	SOCKET nSocket = network_connect(m_sMasterIP.c_str(), m_nListenerPort);
	if(nSocket == INVALID_SOCKET)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't connect to MasterNode! %s:%d\n", __FILE__, __LINE__);
		m_bNeedRestart = true;
		return MARC_CODE_CONNECT_FAILED;
	}

	//发送上传请求消息到服务端
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

	//接收上传请求应答消息
	memset(&msgPacket, 0, sizeof(_stDataPacket));
	if(network_recv(nSocket, (char*)&msgPacket, sizeof(_stDataPacket)) != MARC_NETWORK_OK)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Failed to receive response message of C2L_CMD_UPLOAD_REQ from MasterNode! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return MARC_CODE_RECV_FAILED;
	}
	Net2Host(msgPacket);

	//根据应答的命令进行相应处理
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
	case L2C_CMD_INVALID_CLIENT: //Client无效
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid ClientNode detected by MasterNode, maybe deleted from node queue by MasterNode for network exception! %s:%d\n", __FILE__, __LINE__);
		nRetCode = MARC_CODE_INVALID_CLIENT;
		bNeedRestart = true;
		break;
	default:
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Invalid message command(%d) received from MasterNode! %s:%d\n", msgPacket.nCommand, __FILE__, __LINE__);
		nRetCode = MARC_CODE_INVALID_COMMAND;
		break;
	};

	//断开本次连接
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
	//发送消息告知Master节点
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

	//检查结果目录是否有数据
	CDirScanner ds(m_pConfigure->sOutputDir.c_str(),false);
	const vector<string>& oFileList = ds.GetAllList();
	if(oFileList.empty())
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "Any data file not found in result folder %s! %s:%d\n",
			m_pConfigure->sOutputDir.c_str(), __FILE__, __LINE__);
		return ;
	}

	//获得系统时间，格式YYYYMMDDhhmmss
	string sCurTime = formatDateTime(time(0), 1);

	//结果压缩文件路径(如./result/1_NewsGather_result_20100204095923.myzip)
	char sResultFilePath[512] = {0};
	sprintf(sResultFilePath, "%s%d_%s_result_%s.myzip",
		m_pConfigure->sZipResultDir.c_str(),
		m_pConfigure->nClientID, 
		m_pConfigure->sAppType.c_str(), 
		sCurTime.c_str());

	//执行压缩命令
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

	//检查压缩后的文件是否存在
	if(!DIR_EXIST(sResultFilePath))
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "result-zipfile not found: %s, maybe empty src or myzip failed. [%s:%d]\n", sResultFilePath, __FILE__, __LINE__);
		return ;
	}

	//将结果上传到Result节点
	if(m_pConfigure->nAsynUpload == 1)
	{
		//异步上传方式，将上传的结果文件名放入待上传文件对列
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

			//记下上传失败的文件以便下次系统启动后能重新上传
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
	RememberResultAddr非0时，Client节点会记录首次上传结果数据时向Master请求得到的Result节点地址，
	此后将继续使用该Result节点来上传结果数据（除非某次上传失败）。
	RememberResultAddr为0时，Client节点每次上传结果数据时都将向Master请求得到Result节点地址。
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
	string sRemoteResultFilePath = string(m_pResultNodeAddr->chSavePath) + getFileName(sResultFilePath, true); //Result节点端文件路径
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
		//异步上传方式下，若待上传文件数超过五个则不再获取任务
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
		//同步上传方式下，须等待所有待上传文件都已上传完毕才获取任务
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

	//未设置任务处理命令或命令文件不存在则不请求任务
	if(m_pConfigure->sAppCmd.empty() || !DIR_EXIST(m_pConfigure->sAppCmdFile.c_str()))
	{
		m_pLogger->Write(CRunLogger::LOG_WARNING, "AppCmd file is not specified or not existed: %s\n", m_pConfigure->sAppCmdFile.c_str());
		Sleep(1000);
		return ;
	}

	//上次任务请求失败时间距离当前时间较近时不再请求新任务
	if(time(0) - m_nLastTaskReqFailTime < m_pConfigure->nTaskReqTimeInterval)
	{
		Sleep(1000);
		return ;
	}

	//上次任务完成时间距离当前时间较近则不再请求新任务
	if(time(0) - m_nLastTaskFinishedTime < m_pConfigure->nTaskReqWaitTime)
	{
		Sleep(1000);
		return ;
	}

	//取得任务信息
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
	default: //其他情况
		m_pLogger->Write(CRunLogger::LOG_ERROR, "Can't accquire task(ErrCode: %d) %s:%d\n", nRetCode, __FILE__, __LINE__);
		m_nLastErrCode = nRetCode;
		m_nLastTaskReqFailTime = (int)time(0);
		return ;
	};

	//下载任务压缩文件
	time_t nStartTime = time(0);
	string sLocalTaskZipFilePath = m_pConfigure->sZipTaskDir + getFileName(task.chTaskFile, true); //本地任务压缩文件路径
	nRetCode = sftp_client_download_file(m_sMasterIP.c_str(), task.usPort, task.chTaskFile, sLocalTaskZipFilePath.c_str());
	unsigned int nTimeUsed = (unsigned int)(time(0) - nStartTime);
	switch(nRetCode)
	{
	case SFTP_CODE_OK: //下载成功
		m_pLogger->Write(CRunLogger::LOG_INFO, "Task downloaded successfully(TaskID=%d, TaskFile=%s), %d seconds\n", 
			task.nTaskID, sLocalTaskZipFilePath.c_str(), nTimeUsed);
		m_nCurTaskID = task.nTaskID;
		m_stNodeStatus.nLastFetchedTime = (unsigned int)time(0);
		m_stNodeStatus.nTotalFetchedTasks++;
		m_stNodeStatus.nTotalFecthedTimeUsed += nTimeUsed;
		strcpy(m_stNodeStatus.sLastFetchedFile, sLocalTaskZipFilePath.c_str());
		OnTaskDownloaded(task.nTaskID, sLocalTaskZipFilePath);
		break;
	default: //其他错误
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
	//发送消息告知Master端
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

	//先清空input和output文件夹内容
	CleanDir(m_pConfigure->sInputDir.c_str());
	CleanDir(m_pConfigure->sOutputDir.c_str());

	//解压缩文件到指定的目录(input)
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

	//非阻塞方式启动节点程序,xxx.exe input output ID
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

	//删除任务压缩文件
	if(m_pConfigure->nAutoDeleteTaskFile != 0)
	{
		m_pLogger->Write(CRunLogger::LOG_INFO, "Auto delete task-zipfile: %s\n", sTaskZipFilePath.c_str());
		deleteFile(sTaskZipFilePath.c_str());
	}
}

void CClientNode::OnTaskDownloadFailed(int nTaskID, const string& sTaskZipFilePath)
{
	//发送消息告知Master端
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

	//记录状态（下载失败的不记入执行失败任务数）
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
	//标志文件是否存在
	string sClientStateFlagFile("./state/client.state.ok");
	NormalizePath(sClientStateFlagFile, false);
	if(!DIR_EXIST(sClientStateFlagFile.c_str())) return false;

	//从文件中最多读取state.nBufSize个字节
	string sClientStateFile("./state/client.state");
	NormalizePath(sClientStateFile, false);
	if(!DIR_EXIST(sClientStateFile.c_str())) return false;
	dAppState.nBufSize = readFile(sClientStateFile.c_str(), 0, dAppState.cBuffer, sizeof(dAppState.cBuffer));

	//读取完后删除标志文件
	deleteFile(sClientStateFlagFile.c_str());

	return true;
}

//将未上传的文件名写入临时文件以便下次能重新上传
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

	//读取marc_reupload.result中记录的结果文件到oResultFiles集合
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

	//读取Result目录下未上传的结果文件（AutoDeleteResultFile非0时才有效）
	if(m_pConfigure->nAutoDeleteResultFile != 0)
	{
		CDirScanner ds(m_pConfigure->sZipResultDir.c_str(), false);
		const vector<string>& oZipFiles = ds.GetFileFullList();
		for(size_t i = 0; i < oZipFiles.size(); i++)
		{
			oResultFiles.insert(oZipFiles[i]);
		}
	}

	//清空队列
	LOCK(m_locker);
	while(!m_oUploadFiles.empty())
		m_oUploadFiles.pop();
	UNLOCK(m_locker);

	//存入待上传队列
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
