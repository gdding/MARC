#include "sftp_server.h"
#include "sftp_global.h"
#include "../utils/LoopThread.h"
#include "../utils/Utility.h"
#include "../utils/Network.h"
#include "../utils/RunLogger.h"

class TClientInfo
{
public:
	string sIP;					//客户端IP               
	unsigned short nPort;		//客户端端口
	SOCKET nSocket;				//客户端连接Socket
	int nStartTime;				//连接建立时间
	int nLastActiveTime;		//最近一次活动时间
	FILE *fp;					//文件句柄,首次打开,释放的时候关闭
	string sFilePath;			//文件路径（含文件名）
	int nFileSize;				//文件大小
	int nNextOffset;			//下次读/写的偏移量
	int nFileType;				//0:下载, 1:上传
public:
	TClientInfo()
	{
		sIP = "";
		nPort = 0;
		nSocket = INVALID_SOCKET;
		fp = NULL;
		sFilePath = "";
		nFileSize = 0;
		nNextOffset = 0;
		nFileType = 0;
	}
};

class TServerInfo
{
public:
	SOCKET nListenSocket;  //监听socket
	SOCKET nMaxSocket; //最大套接字
	fd_set fdAllSet;
	int nMaxDataSize; //每次发送的数据最大长度
	int nConnTimeout; //连接维持最大时间
	CLoopThread *pListenThread; //监听线程
	list<TClientInfo*> clients; //客户端队列
	sftp_cb_download_finished fDownloadFinished;
	sftp_cb_upload_finished fUploadFinished;
	void* pUserPrivData;
public:
	TServerInfo()
	{
		nListenSocket = INVALID_SOCKET;
		FD_ZERO(&fdAllSet);
		nMaxDataSize = 4096;
		nConnTimeout = 600;
		pListenThread = NULL;
		clients.clear();
		fDownloadFinished = NULL;
		fUploadFinished = NULL;
		pUserPrivData = NULL;
	}
};

static void ListenRutine(void* param);
static void CloseClient(TServerInfo* pServer, TClientInfo* pClient);
static void MessageHandler(TServerInfo* pServer, TClientInfo *pClient, const _stMsgHeader& msgHeader);
static void SendData(TServerInfo* pServer, TClientInfo *pClient);
static void OnCmdDownloadFileReq(TServerInfo* pServer, TClientInfo *pClient, const _stMsgHeader& msgHeader);
static void OnCmdDownloadDataReq(TServerInfo* pServer, TClientInfo *pClient, const _stMsgHeader& msgHeader);
static void OnCmdUploadFileReq(TServerInfo* pServer, TClientInfo *pClient, const _stMsgHeader& msgHeader);
static void OnCmdUploadDataReq(TServerInfo* pServer, TClientInfo *pClient, const _stMsgHeader& msgHeader);
static void OnCmdClose(TServerInfo* pServer, TClientInfo *pClient, const _stMsgHeader& msgHeader);
static CRunLogger Logger("./log/marc_sftp_server.log", true);

SFTP_SVR* sftp_server_init(const char* ip, unsigned short port, int max_conns)
{
	assert(ip != NULL);
	Logger.Write(CRunLogger::LOG_INFO, "init sftp server, listening on %s:%d\n", ip, port);
	TServerInfo *pServer = NULL;
	SOCKET nListenSocket = network_listener_init(ip, port, max_conns);
	if(nListenSocket != INVALID_SOCKET)
	{
		pServer = new TServerInfo;
		pServer->nListenSocket = nListenSocket;
		pServer->nMaxSocket = nListenSocket;
		FD_ZERO(&pServer->fdAllSet);
		FD_SET(nListenSocket, &pServer->fdAllSet);
		pServer->clients.clear();
		pServer->fDownloadFinished = NULL;
		pServer->fUploadFinished = NULL;
		pServer->pListenThread = new CLoopThread();
		pServer->pListenThread->SetRutine(ListenRutine, pServer);
	}
	return pServer;
}

bool  sftp_server_setopt(SFTP_SVR* h, SFTPoption opt, ...)
{
	bool bRet = false;
	TServerInfo *pServer = (TServerInfo*) h;
	if(pServer != NULL)
	{
		va_list arg;
		va_start(arg, opt);
		switch(opt)
		{
		case SFTPOPT_DOWNLOAD_FINISHED_FUNCTION:
			pServer->fDownloadFinished = va_arg(arg, sftp_cb_download_finished);
			bRet = true;
			break;
		case SFTPOPT_UPLOAD_FINISHED_FUNCTION:
			pServer->fUploadFinished = va_arg(arg, sftp_cb_upload_finished);
			bRet = true;
			break;
		case SFTPOPT_MAX_DATA_PACKET_SIZE:
			pServer->nMaxDataSize = va_arg(arg, int);
			if(pServer->nMaxDataSize <= 0)
				pServer->nMaxDataSize = 4096;
			bRet = true;
			break;
		case SFTPOPT_PRIVATE_DATA:
			pServer->pUserPrivData = va_arg(arg, void*);
			break;
		case SFTPOPT_CONNECTION_TIMEOUT:
			pServer->nConnTimeout = va_arg(arg, int);
			if(pServer->nConnTimeout <= 0)
				pServer->nConnTimeout = 600;
			bRet = true;
			break;
		default:
			break;
		};
		va_end(arg);
	}

	return bRet;
}

bool sftp_server_start(SFTP_SVR* h)
{
	TServerInfo* pServer = (TServerInfo*)h;
	if(pServer == NULL) return false;
	return pServer->pListenThread->Start(0);
}

void sftp_server_stop(SFTP_SVR* h)
{
	TServerInfo* pServer = (TServerInfo*)h;
	if(pServer == NULL) return ;
	assert(pServer->pListenThread != NULL);
	pServer->pListenThread->Stop();

	//关闭各个客户端连接
	list<TClientInfo*>::iterator it = pServer->clients.begin();
	for(; it != pServer->clients.end(); ++it)
	{
		TClientInfo* pClient = (*it);
		assert(pClient != NULL);
		if(pClient->nSocket != INVALID_SOCKET)
		{
			CloseClient(pServer, pClient);
		}
		delete pClient;
	}
	pServer->clients.clear();
}

void sftp_server_exit(SFTP_SVR* h)
{
	TServerInfo* pServer = (TServerInfo*)h;
	if(pServer == NULL) return ;
	
	//关闭监听套接口
	CLOSE_SOCKET(pServer->nListenSocket);
	pServer->nListenSocket = INVALID_SOCKET;
	pServer->nMaxSocket = INVALID_SOCKET;

	delete pServer->pListenThread;
	delete pServer;
}

int sftp_server_active_conns(SFTP_SVR* h)
{
	TServerInfo *pServer = (TServerInfo*)h;
	if(pServer == NULL)
	{
		return 0;
	}
	else
	{
		return pServer->clients.size();
	}
}

void ListenRutine(void* param)
{
	TServerInfo *pServer = (TServerInfo*)param;

	//删除已关闭的连接以及超时的短连接
	list<TClientInfo*>::iterator it = pServer->clients.begin();
	for(; it != pServer->clients.end(); )
	{
		TClientInfo* pClient = (*it);
		assert(pClient != NULL);

		if(pClient->nSocket != INVALID_SOCKET)
		{
			//关闭超时的连接
			bool bConnTimeout = (time(0) - pClient->nLastActiveTime > pServer->nConnTimeout);
			if(bConnTimeout)
			{
				Logger.Write(CRunLogger::LOG_WARNING, "Connection(%s:%d) timeout detected, close it! %s:%d\n", 
					pClient->sIP.c_str(), pClient->nPort, __FILE__, __LINE__);
				CloseClient(pServer, pClient);
			}
		}

		//删除已关闭的客户端
		if(pClient->nSocket == INVALID_SOCKET)
		{
			it = pServer->clients.erase(it);
			delete pClient;
		}
		else
		{
			it++;
		}
	}

	//fdset初始化
	fd_set fdRead, fdException;
	memcpy(&fdRead, &pServer->fdAllSet, sizeof(fd_set));
	memcpy(&fdException, &pServer->fdAllSet, sizeof(fd_set));

	//select操作
	timeval	timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;
	int inReady = select((int)pServer->nMaxSocket+1, &fdRead, NULL, &fdException, &timeout);
	if(inReady < 0) //错误
	{
		Logger.Write(CRunLogger::LOG_ERROR, "select error! %s:%d\n", __FILE__, __LINE__);
		Sleep(2000);
		return ;
	}
	if(inReady == 0) //等待超时，无需处理
	{
		return;
	}

	//若有客户端新连接，则添加到客户端队列中
	if(FD_ISSET(pServer->nListenSocket, &fdRead))
	{
		//得到客户端连接socket
		sockaddr_in addrRemote;
		socklen_t nAddrLen = sizeof(sockaddr_in);
		SOCKET nClientSock = accept(pServer->nListenSocket, (sockaddr*)&addrRemote, &nAddrLen);
		if(nClientSock != INVALID_SOCKET)
		{
			FD_SET(nClientSock, &pServer->fdAllSet);

			//创建客户端对象加入客户端对列
			TClientInfo* pNewClient = new TClientInfo();
			assert(pNewClient != NULL);
			pNewClient->sIP = inet_ntoa(addrRemote.sin_addr);
			pNewClient->nPort = ntohs(addrRemote.sin_port);
			pNewClient->nSocket = nClientSock;
			pNewClient->nStartTime = (int)time(0);
			pNewClient->nLastActiveTime = (int)time(0);
			pServer->clients.push_back(pNewClient);

			//记录最大套接字
			if(pServer->nMaxSocket < nClientSock)
				pServer->nMaxSocket = nClientSock;
		}
		else
		{
			Logger.Write(CRunLogger::LOG_ERROR, "accept failed! %s. %s:%d\n", strerror(errno), __FILE__, __LINE__);
		}

		if(--inReady<=0) return ;
	}

	//接收并处理各个客户端的消息
	it = pServer->clients.begin();
	for(; it != pServer->clients.end() && inReady > 0; ++it)
	{
		TClientInfo* pClient = (*it);
		assert(pClient != NULL);
		assert(pClient->nSocket != INVALID_SOCKET);
		if(FD_ISSET(pClient->nSocket, &fdException)) //异常
		{
			Logger.Write(CRunLogger::LOG_WARNING, "socket exception detected, maybe connection(%s:%d) closed! %s:%d\n",
				pClient->sIP.c_str(), pClient->nPort, __FILE__, __LINE__);
			pClient->nLastActiveTime = (int)time(0);
			CloseClient(pServer, pClient);
			inReady--;
		}
		else if(FD_ISSET(pClient->nSocket, &fdRead)) //有消息来
		{
			pClient->nLastActiveTime = (int)time(0);
			inReady--;

			_stMsgHeader msgHeader;
			memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
			int nRecev = network_recv(pClient->nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE);
			switch(nRecev)
			{
			case MARC_NETWORK_OK:
				Net2Host(msgHeader);
				MessageHandler(pServer, pClient, msgHeader);
				break;
			case MARC_NETWORK_TIMEOUT:
				Logger.Write(CRunLogger::LOG_WARNING, "network_recv timeout! close the connection(%s:%d)! %s:%d\n",
					pClient->sIP.c_str(), pClient->nPort, __FILE__, __LINE__);
				CloseClient(pServer, pClient);
				break ;
			case MARC_NETWORK_CLOSED:
				Logger.Write(CRunLogger::LOG_WARNING, "connection(%s:%d) closed! %s:%d\n",
					pClient->sIP.c_str(), pClient->nPort, __FILE__, __LINE__);
				CloseClient(pServer, pClient);
				break ;
			case MARC_NETWORK_ERROR:
				Logger.Write(CRunLogger::LOG_ERROR, "network error: %s! close the connection(%s:%d)! %s:%d\n",
					strerror(errno), pClient->sIP.c_str(), pClient->nPort, __FILE__, __LINE__);
				CloseClient(pServer, pClient);
				break ;
			};
		}
	}
}

void MessageHandler(TServerInfo* pServer, TClientInfo *pClient, const _stMsgHeader& msgHeader)
{
	assert(pServer != NULL);
	assert(pClient != NULL);
	switch(msgHeader.nCommand)
	{
	case SFTP_CMD_DOWNLOAD_FILE_REQ: //请求下载文件
		OnCmdDownloadFileReq(pServer, pClient, msgHeader);
		break;
	case SFTP_CMD_DOWNLOAD_DATA_REQ: //请求下载数据
		OnCmdDownloadDataReq(pServer, pClient, msgHeader);
		break;
	case SFTP_CMD_UPLOAD_FILE_REQ: //请求上传文件
		OnCmdUploadFileReq(pServer, pClient, msgHeader);
		break;
	case SFTP_CMD_UPLOAD_DATA_REQ: //请求上传数据
		OnCmdUploadDataReq(pServer, pClient, msgHeader);
		break;
	case SFTP_CMD_OK: //传输完成
		OnCmdClose(pServer, pClient, msgHeader);
		break;
	default: //没有这个消息
		Logger.Write(CRunLogger::LOG_WARNING, "Invalid message command: %d, %s:%d\n",
			msgHeader.nCommand, __FILE__, __LINE__);
		CloseClient(pServer, pClient);
		break;
	}//end switch
}

void OnCmdDownloadFileReq(TServerInfo* pServer, TClientInfo *pClient, const _stMsgHeader& msgHeader)
{
	assert(pServer != NULL);
	assert(pClient != NULL);
	_stMsgHeader sendPacket;
	memset(&sendPacket, 0, SFTP_MSG_HEADER_SIZE);
	sendPacket.nCommand  = SFTP_CMD_DOWNLOAD_FILE_NO;
	if(msgHeader.nDataSize > 0) //nDataSize为下载的文件名长度
	{
		//接收请求下载的文件路径
		char *pDataBuf = (char*)malloc(msgHeader.nDataSize+1);
		assert(pDataBuf != NULL);
		memset(pDataBuf, 0, msgHeader.nDataSize+1);
		if(network_recv(pClient->nSocket, pDataBuf, msgHeader.nDataSize) != MARC_NETWORK_OK)
		{
			Logger.Write(CRunLogger::LOG_WARNING, "network_recv excetion detected, close the connection(%s:%d)! %s:%d\n",
				pClient->sIP.c_str(), pClient->nPort, __FILE__, __LINE__);
			CloseClient(pServer, pClient);
			free(pDataBuf);
			return ;
		}
		string sFilePath = pDataBuf;
		free(pDataBuf);
		//printf("sFilePath = %s\n", sFilePath.c_str());

		//打开文件，准备读取
		FILE *fp = fopen(sFilePath.c_str(), "rb");
		if(fp != NULL)
		{
			sendPacket.nCommand = SFTP_CMD_DOWNLOAD_FILE_YES;
			sendPacket.nDataSize = pServer->nMaxDataSize; //服务端允许每次传输的最大数据块长度
			fseek(fp, 0L, SEEK_END);
			int nFileSize = ftell(fp);
			sendPacket.nOffset =  nFileSize; //回传的nOffset表示该文件大小
			fseek(fp, 0L, SEEK_SET);
			pClient->fp = fp;
			pClient->sFilePath = sFilePath;
			pClient->nFileSize = nFileSize;
			pClient->nNextOffset = 0;
			pClient->nFileType = 0;
		}
		else
		{
			Logger.Write(CRunLogger::LOG_ERROR, "Can't read the file: %s, %s:%d\n", sFilePath.c_str(), __FILE__, __LINE__);
		}
	}

	//给客户端发送确认消息
	Host2Net(sendPacket);
	if(network_send(pClient->nSocket, (char *)&sendPacket, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "network_send failed! %s:%d\n", __FILE__, __LINE__);
		CloseClient(pServer, pClient);
	}

	//此后进入SFTP_CMD_DOWNLOAD_DATA_REQ阶段
}

void OnCmdDownloadDataReq(TServerInfo* pServer, TClientInfo *pClient, const _stMsgHeader& msgHeader)
{
	assert(pServer != NULL);
	assert(pClient != NULL);
	assert(pClient->fp != NULL);
	_stMsgHeader sendPacket;
	memset(&sendPacket, 0, SFTP_MSG_HEADER_SIZE);
	if(msgHeader.nOffset != pClient->nNextOffset)
	{
		//出现异常
		Logger.Write(CRunLogger::LOG_ERROR, "Dismatched data offset! %s:%d\n", __FILE__, __LINE__);
		sendPacket.nCommand = SFTP_CMD_DOWNLOAD_DATA_NO;
		Host2Net(sendPacket);
		if(network_send(pClient->nSocket, (char *)&sendPacket, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "network_send failed! %s:%d\n", __FILE__, __LINE__);
			CloseClient(pServer, pClient);
		}
	}
	else
	{
		//文件数据缓冲区
		char* pDataBuf = (char*)malloc(pServer->nMaxDataSize);
		assert(pDataBuf != NULL);
		memset(pDataBuf, 0, pServer->nMaxDataSize);

		//移动文件指针到指定偏移位置读取数据
		fseek(pClient->fp, msgHeader.nOffset, SEEK_SET);
		int nDataSize = (int)fread(pDataBuf, sizeof(char), pServer->nMaxDataSize, pClient->fp); 
		assert(nDataSize > 0);

		//发送确认消息,失败则断开连接
		sendPacket.nCommand = SFTP_CMD_DOWNLOAD_DATA_YES;
		sendPacket.nOffset = pClient->nNextOffset;
		sendPacket.nDataSize = nDataSize;
		Host2Net(sendPacket);
		if(network_send(pClient->nSocket, (char *)&sendPacket, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "network_send failed! %s:%d\n", __FILE__, __LINE__);
			CloseClient(pServer, pClient);
			free(pDataBuf);
			return ;
		}

		//发送文件数据,失败则断开连接
		//if(network_send(pClient->nSocket, pDataBuf, sendPacket.nDataSize) != MARC_NETWORK_OK)
		if(network_send(pClient->nSocket, pDataBuf, nDataSize) != MARC_NETWORK_OK)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "network_send failed! %s:%d\n", __FILE__, __LINE__);
			CloseClient(pServer, pClient);
			free(pDataBuf);
			return ;
		}
		pClient->nNextOffset += nDataSize;
	
		free(pDataBuf);
	}
}

void OnCmdUploadFileReq(TServerInfo* pServer, TClientInfo *pClient, const _stMsgHeader& msgHeader)
{
	_stMsgHeader sendPacket;
	memset(&sendPacket, 0, SFTP_MSG_HEADER_SIZE);
	sendPacket.nCommand  = SFTP_CMD_UPLOAD_FILE_NO;
	if(msgHeader.nDataSize > 0)
	{
		//接收请求上传到的文件名（含路径）
		char *pDataBuf = (char*)malloc(msgHeader.nDataSize+1);
		assert(pDataBuf != NULL);
		memset(pDataBuf, 0, msgHeader.nDataSize+1);
		if(network_recv(pClient->nSocket, pDataBuf, msgHeader.nDataSize) != MARC_NETWORK_OK)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "network_recv failed! %s:%d\n", __FILE__, __LINE__);
			CloseClient(pServer, pClient);
			free(pDataBuf);
			return ;
		}
		string sFilePath = pDataBuf;
		free(pDataBuf);
		//printf("OnCmdUploadReq: sFilePath = %s\n", sFilePath.c_str());

		//创建路径，新建服务端本地文件
		if(CreateFilePath(sFilePath.c_str()))
		{
			FILE *fp = fopen(sFilePath.c_str(), "wb");
			if(fp != NULL)
			{
				sendPacket.nCommand = SFTP_CMD_UPLOAD_FILE_YES;
				sendPacket.nOffset = 0;
				sendPacket.nDataSize = pServer->nMaxDataSize;
				pClient->fp = fp;
				pClient->sFilePath = sFilePath;
				pClient->nFileSize = msgHeader.nOffset; //文件大小
				pClient->nNextOffset = 0;
				pClient->nFileType = 1;
			}
			else
			{
				Logger.Write(CRunLogger::LOG_ERROR, "Can't create file: %s, %s:%d\n", sFilePath.c_str(), __FILE__, __LINE__);
				CloseClient(pServer, pClient);
				return ;
			}
		}
		else
		{
			Logger.Write(CRunLogger::LOG_ERROR, "CreateFilePath failed: %s, %s:%d\n", sFilePath.c_str(), __FILE__, __LINE__);
			CloseClient(pServer, pClient);
			return ;
		}
	}

	//发送确认消息
	Host2Net(sendPacket);
	if(network_send(pClient->nSocket, (char *)&sendPacket, SFTP_MSG_HEADER_SIZE)!=MARC_NETWORK_OK)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "network_send failed! %s:%d\n", __FILE__, __LINE__);
		CloseClient(pServer, pClient);
	}

	//请求之后进入SFTP_CMD_UPLOAD_DATA阶段
}

void OnCmdUploadDataReq(TServerInfo* pServer, TClientInfo *pClient, const _stMsgHeader& msgHeader)
{
	int nDataSize = msgHeader.nDataSize; //数据块的大小
	int nOffset = msgHeader.nOffset; //该数据块在文件中的偏移量
	assert(nDataSize > 0);
	assert(nOffset == pClient->nNextOffset);

	//返回给客户端的确认消息包
	_stMsgHeader sendPacket;
	memset(&sendPacket, 0, SFTP_MSG_HEADER_SIZE);

	//接收数据
	char* pDataBuf = (char*)malloc(nDataSize);
	assert(pDataBuf != NULL);
	if(network_recv(pClient->nSocket, pDataBuf, nDataSize) != MARC_NETWORK_OK)
	{
		//异常情况(可能是客户端直接关闭)
		Logger.Write(CRunLogger::LOG_ERROR, "network_recv failed! %s:%d\n", __FILE__, __LINE__);
		free(pDataBuf);
		CloseClient(pServer, pClient);
		return ;
	}

	//printf("接收到一块数据, nOffset = %d, nDataSize = %d\n", nOffset, nDataSize);

	//将数据写入文件
	assert(pClient->fp != NULL);
	int nWriteCount = (int)fwrite(pDataBuf, sizeof(char), nDataSize, pClient->fp);
	free(pDataBuf);
	if(nWriteCount != nDataSize) //极少出现
	{
		Logger.Write(CRunLogger::LOG_ERROR, "fwrite failed! %s:%d\n", __FILE__, __LINE__);
		sendPacket.nCommand = SFTP_CMD_UPLOAD_DATA_NO;
	}
	else
	{
		pClient->nNextOffset += nDataSize;
		sendPacket.nCommand = SFTP_CMD_UPLOAD_DATA_YES;
		sendPacket.nOffset = pClient->nNextOffset;
	}

	//发送确认消息
	Host2Net(sendPacket);
	if(network_send(pClient->nSocket, (char *)&sendPacket, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK)
	{
		//异常情况(可能是客户端直接关闭)
		Logger.Write(CRunLogger::LOG_ERROR, "network_send failed! %s:%d\n", __FILE__, __LINE__);
		CloseClient(pServer, pClient);
	}
}

void OnCmdClose(TServerInfo* pServer, TClientInfo *pClient, const _stMsgHeader& msgHeader)
{
	switch(pClient->nFileType)
	{
	case 0: //执行到这表示客户端下载文件成功
		if(pClient->fp != NULL)
		{
			fclose(pClient->fp);
			pClient->fp = NULL;
			if(pServer->fDownloadFinished != NULL)
				pServer->fDownloadFinished(pServer->pUserPrivData, pClient->sFilePath.c_str());
			pClient->sFilePath = "";
		}
		break;
	case 1: //执行到这表示客户端上传文件成功
		if(pClient->fp != NULL)
		{
			fclose(pClient->fp);
			pClient->fp = NULL;
			if(pServer->fUploadFinished != NULL)
				pServer->fUploadFinished(pServer->pUserPrivData, pClient->sFilePath.c_str());
			pClient->sFilePath = "";
		}
		break;
	}

	//关闭连接
	CloseClient(pServer, pClient);
}

void CloseClient(TServerInfo* pServer, TClientInfo* pClient)
{
	assert(pClient != NULL);
	FD_CLR(pClient->nSocket, &pServer->fdAllSet);
	CLOSE_SOCKET(pClient->nSocket);
	pClient->nSocket = INVALID_SOCKET;
	if(pClient->fp != NULL)
	{
		fclose(pClient->fp);
		pClient->fp = NULL;
		if(pClient->nFileType == 1)
			deleteFile(pClient->sFilePath.c_str());
	}
	pClient->sFilePath = "";
	pClient->nFileSize = 0;
	pClient->nNextOffset = 0;
}
