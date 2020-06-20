#include "sftp_client.h"
#include "sftp_global.h"
#include "../utils/Utility.h"
#include "../utils/Network.h"
#include "../utils/RunLogger.h"

static int Save2LocalFile(SOCKET nSocket, const char* sLocalFile, int nFileSize, int nMaxDataSize);
static int UploadLocalFile(SOCKET nSocket, const char* sLocalFile, int nFileSize, int nMaxDataSize);
static CRunLogger Logger("./log/marc_sftp_client.log", true);

int sftp_client_download_file(const char* svr_ip, unsigned short svr_port, const char* remote_filepath, const char* local_filepath)
{
	//连接服务端
	SOCKET nSocket = network_connect(svr_ip, svr_port);
	if(nSocket == INVALID_SOCKET)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "Can't connect to the sftp server(%s:%d), %s:%d\n",
			svr_ip, svr_port, __FILE__, __LINE__);
		return SFTP_CODE_CONNECT_FAILED;
	}

	//发送文件下载请求消息
	_stMsgHeader msgHeader;
	memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
	msgHeader.nCommand = SFTP_CMD_DOWNLOAD_FILE_REQ;
	msgHeader.nDataSize = (int)strlen(remote_filepath);
	Host2Net(msgHeader);
	if(network_send(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK) //消息头
	{
		Logger.Write(CRunLogger::LOG_ERROR, "network_send() failed! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_SEND_FAILED;
	}
	if(network_send(nSocket, (char*)remote_filepath, strlen(remote_filepath)) != MARC_NETWORK_OK) //消息内容
	{
		Logger.Write(CRunLogger::LOG_ERROR, "network_send() failed! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_SEND_FAILED;
	}

	//接收并处理服务端返回的确认消息
	memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
	if(network_recv(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "network_recv() failed! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_RECV_FAILED;
	}
	Net2Host(msgHeader);
	int nFileSize = 0;
	int nMaxDataSize = 0;
	int nRetCode = SFTP_CODE_OK;
	switch(msgHeader.nCommand)
	{
	case SFTP_CMD_DOWNLOAD_FILE_YES:
		nFileSize = msgHeader.nOffset; //文件大小
		nMaxDataSize = msgHeader.nDataSize; //服务端允许每次传输数据的最大值
		nRetCode = Save2LocalFile(nSocket, local_filepath, nFileSize, nMaxDataSize);
		if(nRetCode == SFTP_CODE_OK) //下载成功
		{
			memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
			msgHeader.nCommand = SFTP_CMD_OK;
			msgHeader.nOffset = nFileSize;
			Host2Net(msgHeader);
			network_send(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE);
			//Sleep(2000);
		}
		else //下载失败
		{
		}
		CLOSE_SOCKET(nSocket);
		return nRetCode;
	case SFTP_CMD_DOWNLOAD_FILE_NO:
		Logger.Write(CRunLogger::LOG_ERROR, "sftp server returned SFTP_CMD_DOWNLOAD_FILE_NO! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_FILE_ERROR;
	default:
		Logger.Write(CRunLogger::LOG_ERROR, "Invalid command: %d, %s:%d\n", msgHeader.nCommand, __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_INVALID_COMMAND;
	};
}

int sftp_client_upload_file(const char* svr_ip, unsigned short svr_port, const char* local_filepath, const char* remote_filepath)
{
	//连接服务端
	SOCKET nSocket = network_connect(svr_ip, svr_port);
	if(nSocket == INVALID_SOCKET)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "Can't connect to the sftp server(%s:%d), %s:%d\n",
			svr_ip, svr_port, __FILE__, __LINE__);
		return SFTP_CODE_CONNECT_FAILED; 
	}

	//向服务端发送文件上传请求
	int nFileSize = getFileSize(local_filepath);
	if(nFileSize < 0) 
	{
		Logger.Write(CRunLogger::LOG_ERROR, "File not existed or read error: %s, %s:%d\n",
			local_filepath, __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_FILE_ERROR;
	}
	_stMsgHeader msgHeader;
	memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
	msgHeader.nCommand = SFTP_CMD_UPLOAD_FILE_REQ;
	msgHeader.nOffset = nFileSize; //此时nOffset为文件大小
	msgHeader.nDataSize = (int)strlen(remote_filepath);
	Host2Net(msgHeader);
	if(network_send(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK) //消息头
	{
		Logger.Write(CRunLogger::LOG_ERROR, "network_send() failed! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_SEND_FAILED;
	}
//	if(network_send(nSocket, (char*)remote_filepath, msgHeader.nDataSize) != MARC_NETWORK_OK) //消息内容
	if(network_send(nSocket, (char*)remote_filepath, strlen(remote_filepath)) != MARC_NETWORK_OK)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "network_send() failed! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_SEND_FAILED;
	}

	//接收并处理服务端返回的确认消息
	memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
	if(network_recv(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "network_recv() failed! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_RECV_FAILED;
	}
	Net2Host(msgHeader);
	int nRetCode = SFTP_CODE_OK;
	int nMaxDataSize = 0;
	switch(msgHeader.nCommand)
	{
	case SFTP_CMD_UPLOAD_FILE_YES: //请求成功，开始传输文件
		nMaxDataSize = msgHeader.nDataSize; //服务端允许每次传输数据的最大值
		assert(nMaxDataSize > 0);
		nRetCode = UploadLocalFile(nSocket, local_filepath, nFileSize, nMaxDataSize);
		break;
	case SFTP_CMD_UPLOAD_FILE_NO: //请求失败
		Logger.Write(CRunLogger::LOG_ERROR, "sftp server returned SFTP_CMD_UPLOAD_FILE_NO! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_FILE_ERROR;
	default:
		Logger.Write(CRunLogger::LOG_ERROR, "Invalid command: %d, %s:%d\n", msgHeader.nCommand, __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_INVALID_COMMAND;
	};

	//上传结束
	switch(nRetCode)
	{
	case SFTP_CODE_OK: //上传成功
		memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
		msgHeader.nCommand = SFTP_CMD_OK;
		msgHeader.nOffset = nFileSize;
		Host2Net(msgHeader);
		network_send(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE);
		//Sleep(2000);
		CLOSE_SOCKET(nSocket);
		break;
	default: //其他情况直接关闭
		CLOSE_SOCKET(nSocket);
		break;	
	}
	
	return nRetCode;
}

int Save2LocalFile(SOCKET nSocket, const char* sLocalFile, int nFileSize, int nMaxDataSize)
{
	int nRetCode = SFTP_CODE_OK;

	//创建路径
	if(!CreateFilePath(sLocalFile))
	{
		Logger.Write(CRunLogger::LOG_ERROR, "Can't create path: %s, %s:%d\n", sLocalFile, __FILE__, __LINE__);
		return SFTP_CODE_FILE_ERROR;
	}

	//创建本地文件
	FILE *fp = fopen(sLocalFile, "wb");
	if(fp == NULL)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "Can't create file: %s, %s:%d\n", sLocalFile, __FILE__, __LINE__);
		return SFTP_CODE_FILE_ERROR;
	}

	assert(nMaxDataSize > 0);
	char* pDataBuf = (char*)malloc(nMaxDataSize);
	assert(pDataBuf != NULL);
	memset(pDataBuf, 0, nMaxDataSize);

	//循环下载数据, 直到完成或者发生异常
	_stMsgHeader msgHeader;
	int nNextOffset = 0;
	while(nNextOffset < nFileSize)
	{
		//发送数据下载请求消息
		memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
		msgHeader.nCommand = SFTP_CMD_DOWNLOAD_DATA_REQ;
		msgHeader.nOffset = nNextOffset;
		Host2Net(msgHeader);
		if(network_send(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "network_send() failed! %s:%d\n", __FILE__, __LINE__);
			nRetCode = SFTP_CODE_SEND_FAILED;
			break;
		}

		//接收请求确认消息
		memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
		if(network_recv(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "network_recv() failed! %s:%d\n", __FILE__, __LINE__);
			nRetCode = SFTP_CODE_RECV_FAILED;
			break;
		}
		Net2Host(msgHeader);
		if(msgHeader.nCommand != SFTP_CMD_DOWNLOAD_DATA_YES)
		{
			//这种情况应该不会出现
			assert(false);
			nRetCode = SFTP_CODE_FILE_ERROR;
			break;
		}

		//接收下载的数据块
		assert(msgHeader.nCommand == SFTP_CMD_DOWNLOAD_DATA_YES);
		assert(msgHeader.nOffset == nNextOffset);
		assert(msgHeader.nDataSize > 0);
		if(network_recv(nSocket, pDataBuf, msgHeader.nDataSize) != MARC_NETWORK_OK)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "network_recv() failed! %s:%d\n", __FILE__, __LINE__);
			nRetCode = SFTP_CODE_RECV_FAILED;
			break;
		}
		//printf("接收到一块数据，nOffset = %d, DataSize = %d\n", nNextOffset, msgHeader.nDataSize);

		//将接收到的数据保存到文件
		int nWriteBytes = (int)fwrite(pDataBuf, sizeof(char), msgHeader.nDataSize, fp);
		if(nWriteBytes != msgHeader.nDataSize)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "fwrite() exception, disk error or free space not enough? %s:%d\n", __FILE__, __LINE__);
			nRetCode = SFTP_CODE_FILE_ERROR;
			break;
		}

		//下次请求下载的数据偏移量
		nNextOffset += msgHeader.nDataSize;
	}
	free(pDataBuf);
	fclose(fp);

	//若下载不成功则删除文件
	if(nRetCode != SFTP_CODE_OK)
	{
		deleteFile(sLocalFile);
	}

	return nRetCode;
}

int UploadLocalFile(SOCKET nSocket, const char* sLocalFile, int nFileSize, int nMaxDataSize)
{
	int nRetCode = SFTP_CODE_OK;

	//打开该文件，记录文件大小
	FILE *fp = fopen(sLocalFile, "rb");
	if(fp == NULL)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "Can't read the file: %s, not existed? %s:%d\n", sLocalFile, __FILE__, __LINE__);
		return SFTP_CODE_FILE_ERROR;
	}

	//文件数据缓冲区
	assert(nMaxDataSize > 0);
	char* pDataBuf = (char*)malloc(nMaxDataSize);
	assert(pDataBuf != NULL);
	memset(pDataBuf, 0, nMaxDataSize);

	//循环发送数据，直到完毕或发生异常
	int nNextOffset = 0; //下次读取的偏移位置
	while(nNextOffset < nFileSize)
	{
		//移动文件指针到指定偏移位置读取数据
		fseek(fp, nNextOffset, SEEK_SET);
		int nDataSize = (int)fread(pDataBuf, sizeof(char), nMaxDataSize, fp);
		assert(nDataSize > 0);

		//发送数据上传请求
		_stMsgHeader msgHeader;
		memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
		msgHeader.nOffset = nNextOffset; //该块数据在文件中的偏移量
		msgHeader.nDataSize =  nDataSize; //数据大小
		msgHeader.nCommand = SFTP_CMD_UPLOAD_DATA_REQ;
		Host2Net(msgHeader);
		if(network_send(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "network_send() failed! %s:%d\n", __FILE__, __LINE__);
			nRetCode = SFTP_CODE_SEND_FAILED;
			break;
		}

		//发送文件数据
		if(network_send(nSocket, pDataBuf, nDataSize) != MARC_NETWORK_OK)
		{
			//发送失败，不再继续发送
			Logger.Write(CRunLogger::LOG_ERROR, "network_send() failed! %s:%d\n", __FILE__, __LINE__);
			nRetCode = SFTP_CODE_SEND_FAILED;
			break;
		}

		//接收服务端返回的确认消息（只有确认成功才继续发送）
		memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
		if(network_recv(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK)
		{
			//接收失败，不再继续发送
			Logger.Write(CRunLogger::LOG_ERROR, "network_recv() failed! %s:%d\n", __FILE__, __LINE__);
			nRetCode = SFTP_CODE_RECV_FAILED;
			break;
		}
		Net2Host(msgHeader);

		//处理确认消息
		if(msgHeader.nCommand == SFTP_CMD_UPLOAD_DATA_YES)
		{
			//下次读取的偏移位置
			nNextOffset = msgHeader.nOffset;
		}
		else
		{
			assert(msgHeader.nCommand == SFTP_CMD_UPLOAD_DATA_NO);
			Logger.Write(CRunLogger::LOG_ERROR, "sftp server return SFTP_CMD_UPLOAD_DATA_NO! %s:%d\n", __FILE__, __LINE__);
			nRetCode = SFTP_CODE_FILE_ERROR;
			break;
		}
	}
	fclose(fp);
	free(pDataBuf);

	return nRetCode;
}
