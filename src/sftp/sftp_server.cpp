#include "sftp_server.h"
#include "sftp_global.h"
#include "../utils/LoopThread.h"
#include "../utils/Utility.h"
#include "../utils/Network.h"
#include "../utils/RunLogger.h"

class TClientInfo
{
public:
	string sIP;					//�ͻ���IP               
	unsigned short nPort;		//�ͻ��˶˿�
	SOCKET nSocket;				//�ͻ�������Socket
	int nStartTime;				//���ӽ���ʱ��
	int nLastActiveTime;		//���һ�λʱ��
	FILE *fp;					//�ļ����,�״δ�,�ͷŵ�ʱ��ر�
	string sFilePath;			//�ļ�·�������ļ�����
	int nFileSize;				//�ļ���С
	int nNextOffset;			//�´ζ�/д��ƫ����
	int nFileType;				//0:����, 1:�ϴ�
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
	SOCKET nListenSocket;  //����socket
	SOCKET nMaxSocket; //����׽���
	fd_set fdAllSet;
	int nMaxDataSize; //ÿ�η��͵�������󳤶�
	int nConnTimeout; //����ά�����ʱ��
	CLoopThread *pListenThread; //�����߳�
	list<TClientInfo*> clients; //�ͻ��˶���
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

	//�رո����ͻ�������
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
	
	//�رռ����׽ӿ�
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

	//ɾ���ѹرյ������Լ���ʱ�Ķ�����
	list<TClientInfo*>::iterator it = pServer->clients.begin();
	for(; it != pServer->clients.end(); )
	{
		TClientInfo* pClient = (*it);
		assert(pClient != NULL);

		if(pClient->nSocket != INVALID_SOCKET)
		{
			//�رճ�ʱ������
			bool bConnTimeout = (time(0) - pClient->nLastActiveTime > pServer->nConnTimeout);
			if(bConnTimeout)
			{
				Logger.Write(CRunLogger::LOG_WARNING, "Connection(%s:%d) timeout detected, close it! %s:%d\n", 
					pClient->sIP.c_str(), pClient->nPort, __FILE__, __LINE__);
				CloseClient(pServer, pClient);
			}
		}

		//ɾ���ѹرյĿͻ���
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

	//fdset��ʼ��
	fd_set fdRead, fdException;
	memcpy(&fdRead, &pServer->fdAllSet, sizeof(fd_set));
	memcpy(&fdException, &pServer->fdAllSet, sizeof(fd_set));

	//select����
	timeval	timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;
	int inReady = select((int)pServer->nMaxSocket+1, &fdRead, NULL, &fdException, &timeout);
	if(inReady < 0) //����
	{
		Logger.Write(CRunLogger::LOG_ERROR, "select error! %s:%d\n", __FILE__, __LINE__);
		Sleep(2000);
		return ;
	}
	if(inReady == 0) //�ȴ���ʱ�����账��
	{
		return;
	}

	//���пͻ��������ӣ�����ӵ��ͻ��˶�����
	if(FD_ISSET(pServer->nListenSocket, &fdRead))
	{
		//�õ��ͻ�������socket
		sockaddr_in addrRemote;
		socklen_t nAddrLen = sizeof(sockaddr_in);
		SOCKET nClientSock = accept(pServer->nListenSocket, (sockaddr*)&addrRemote, &nAddrLen);
		if(nClientSock != INVALID_SOCKET)
		{
			FD_SET(nClientSock, &pServer->fdAllSet);

			//�����ͻ��˶������ͻ��˶���
			TClientInfo* pNewClient = new TClientInfo();
			assert(pNewClient != NULL);
			pNewClient->sIP = inet_ntoa(addrRemote.sin_addr);
			pNewClient->nPort = ntohs(addrRemote.sin_port);
			pNewClient->nSocket = nClientSock;
			pNewClient->nStartTime = (int)time(0);
			pNewClient->nLastActiveTime = (int)time(0);
			pServer->clients.push_back(pNewClient);

			//��¼����׽���
			if(pServer->nMaxSocket < nClientSock)
				pServer->nMaxSocket = nClientSock;
		}
		else
		{
			Logger.Write(CRunLogger::LOG_ERROR, "accept failed! %s. %s:%d\n", strerror(errno), __FILE__, __LINE__);
		}

		if(--inReady<=0) return ;
	}

	//���ղ���������ͻ��˵���Ϣ
	it = pServer->clients.begin();
	for(; it != pServer->clients.end() && inReady > 0; ++it)
	{
		TClientInfo* pClient = (*it);
		assert(pClient != NULL);
		assert(pClient->nSocket != INVALID_SOCKET);
		if(FD_ISSET(pClient->nSocket, &fdException)) //�쳣
		{
			Logger.Write(CRunLogger::LOG_WARNING, "socket exception detected, maybe connection(%s:%d) closed! %s:%d\n",
				pClient->sIP.c_str(), pClient->nPort, __FILE__, __LINE__);
			pClient->nLastActiveTime = (int)time(0);
			CloseClient(pServer, pClient);
			inReady--;
		}
		else if(FD_ISSET(pClient->nSocket, &fdRead)) //����Ϣ��
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
	case SFTP_CMD_DOWNLOAD_FILE_REQ: //���������ļ�
		OnCmdDownloadFileReq(pServer, pClient, msgHeader);
		break;
	case SFTP_CMD_DOWNLOAD_DATA_REQ: //������������
		OnCmdDownloadDataReq(pServer, pClient, msgHeader);
		break;
	case SFTP_CMD_UPLOAD_FILE_REQ: //�����ϴ��ļ�
		OnCmdUploadFileReq(pServer, pClient, msgHeader);
		break;
	case SFTP_CMD_UPLOAD_DATA_REQ: //�����ϴ�����
		OnCmdUploadDataReq(pServer, pClient, msgHeader);
		break;
	case SFTP_CMD_OK: //�������
		OnCmdClose(pServer, pClient, msgHeader);
		break;
	default: //û�������Ϣ
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
	if(msgHeader.nDataSize > 0) //nDataSizeΪ���ص��ļ�������
	{
		//�����������ص��ļ�·��
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

		//���ļ���׼����ȡ
		FILE *fp = fopen(sFilePath.c_str(), "rb");
		if(fp != NULL)
		{
			sendPacket.nCommand = SFTP_CMD_DOWNLOAD_FILE_YES;
			sendPacket.nDataSize = pServer->nMaxDataSize; //���������ÿ�δ����������ݿ鳤��
			fseek(fp, 0L, SEEK_END);
			int nFileSize = ftell(fp);
			sendPacket.nOffset =  nFileSize; //�ش���nOffset��ʾ���ļ���С
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

	//���ͻ��˷���ȷ����Ϣ
	Host2Net(sendPacket);
	if(network_send(pClient->nSocket, (char *)&sendPacket, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "network_send failed! %s:%d\n", __FILE__, __LINE__);
		CloseClient(pServer, pClient);
	}

	//�˺����SFTP_CMD_DOWNLOAD_DATA_REQ�׶�
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
		//�����쳣
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
		//�ļ����ݻ�����
		char* pDataBuf = (char*)malloc(pServer->nMaxDataSize);
		assert(pDataBuf != NULL);
		memset(pDataBuf, 0, pServer->nMaxDataSize);

		//�ƶ��ļ�ָ�뵽ָ��ƫ��λ�ö�ȡ����
		fseek(pClient->fp, msgHeader.nOffset, SEEK_SET);
		int nDataSize = (int)fread(pDataBuf, sizeof(char), pServer->nMaxDataSize, pClient->fp); 
		assert(nDataSize > 0);

		//����ȷ����Ϣ,ʧ����Ͽ�����
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

		//�����ļ�����,ʧ����Ͽ�����
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
		//���������ϴ������ļ�������·����
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

		//����·�����½�����˱����ļ�
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
				pClient->nFileSize = msgHeader.nOffset; //�ļ���С
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

	//����ȷ����Ϣ
	Host2Net(sendPacket);
	if(network_send(pClient->nSocket, (char *)&sendPacket, SFTP_MSG_HEADER_SIZE)!=MARC_NETWORK_OK)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "network_send failed! %s:%d\n", __FILE__, __LINE__);
		CloseClient(pServer, pClient);
	}

	//����֮�����SFTP_CMD_UPLOAD_DATA�׶�
}

void OnCmdUploadDataReq(TServerInfo* pServer, TClientInfo *pClient, const _stMsgHeader& msgHeader)
{
	int nDataSize = msgHeader.nDataSize; //���ݿ�Ĵ�С
	int nOffset = msgHeader.nOffset; //�����ݿ����ļ��е�ƫ����
	assert(nDataSize > 0);
	assert(nOffset == pClient->nNextOffset);

	//���ظ��ͻ��˵�ȷ����Ϣ��
	_stMsgHeader sendPacket;
	memset(&sendPacket, 0, SFTP_MSG_HEADER_SIZE);

	//��������
	char* pDataBuf = (char*)malloc(nDataSize);
	assert(pDataBuf != NULL);
	if(network_recv(pClient->nSocket, pDataBuf, nDataSize) != MARC_NETWORK_OK)
	{
		//�쳣���(�����ǿͻ���ֱ�ӹر�)
		Logger.Write(CRunLogger::LOG_ERROR, "network_recv failed! %s:%d\n", __FILE__, __LINE__);
		free(pDataBuf);
		CloseClient(pServer, pClient);
		return ;
	}

	//printf("���յ�һ������, nOffset = %d, nDataSize = %d\n", nOffset, nDataSize);

	//������д���ļ�
	assert(pClient->fp != NULL);
	int nWriteCount = (int)fwrite(pDataBuf, sizeof(char), nDataSize, pClient->fp);
	free(pDataBuf);
	if(nWriteCount != nDataSize) //���ٳ���
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

	//����ȷ����Ϣ
	Host2Net(sendPacket);
	if(network_send(pClient->nSocket, (char *)&sendPacket, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK)
	{
		//�쳣���(�����ǿͻ���ֱ�ӹر�)
		Logger.Write(CRunLogger::LOG_ERROR, "network_send failed! %s:%d\n", __FILE__, __LINE__);
		CloseClient(pServer, pClient);
	}
}

void OnCmdClose(TServerInfo* pServer, TClientInfo *pClient, const _stMsgHeader& msgHeader)
{
	switch(pClient->nFileType)
	{
	case 0: //ִ�е����ʾ�ͻ��������ļ��ɹ�
		if(pClient->fp != NULL)
		{
			fclose(pClient->fp);
			pClient->fp = NULL;
			if(pServer->fDownloadFinished != NULL)
				pServer->fDownloadFinished(pServer->pUserPrivData, pClient->sFilePath.c_str());
			pClient->sFilePath = "";
		}
		break;
	case 1: //ִ�е����ʾ�ͻ����ϴ��ļ��ɹ�
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

	//�ر�����
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
