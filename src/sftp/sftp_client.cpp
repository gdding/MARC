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
	//���ӷ����
	SOCKET nSocket = network_connect(svr_ip, svr_port);
	if(nSocket == INVALID_SOCKET)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "Can't connect to the sftp server(%s:%d), %s:%d\n",
			svr_ip, svr_port, __FILE__, __LINE__);
		return SFTP_CODE_CONNECT_FAILED;
	}

	//�����ļ�����������Ϣ
	_stMsgHeader msgHeader;
	memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
	msgHeader.nCommand = SFTP_CMD_DOWNLOAD_FILE_REQ;
	msgHeader.nDataSize = (int)strlen(remote_filepath);
	Host2Net(msgHeader);
	if(network_send(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK) //��Ϣͷ
	{
		Logger.Write(CRunLogger::LOG_ERROR, "network_send() failed! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_SEND_FAILED;
	}
	if(network_send(nSocket, (char*)remote_filepath, strlen(remote_filepath)) != MARC_NETWORK_OK) //��Ϣ����
	{
		Logger.Write(CRunLogger::LOG_ERROR, "network_send() failed! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_SEND_FAILED;
	}

	//���ղ��������˷��ص�ȷ����Ϣ
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
		nFileSize = msgHeader.nOffset; //�ļ���С
		nMaxDataSize = msgHeader.nDataSize; //���������ÿ�δ������ݵ����ֵ
		nRetCode = Save2LocalFile(nSocket, local_filepath, nFileSize, nMaxDataSize);
		if(nRetCode == SFTP_CODE_OK) //���سɹ�
		{
			memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
			msgHeader.nCommand = SFTP_CMD_OK;
			msgHeader.nOffset = nFileSize;
			Host2Net(msgHeader);
			network_send(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE);
			//Sleep(2000);
		}
		else //����ʧ��
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
	//���ӷ����
	SOCKET nSocket = network_connect(svr_ip, svr_port);
	if(nSocket == INVALID_SOCKET)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "Can't connect to the sftp server(%s:%d), %s:%d\n",
			svr_ip, svr_port, __FILE__, __LINE__);
		return SFTP_CODE_CONNECT_FAILED; 
	}

	//�����˷����ļ��ϴ�����
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
	msgHeader.nOffset = nFileSize; //��ʱnOffsetΪ�ļ���С
	msgHeader.nDataSize = (int)strlen(remote_filepath);
	Host2Net(msgHeader);
	if(network_send(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK) //��Ϣͷ
	{
		Logger.Write(CRunLogger::LOG_ERROR, "network_send() failed! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_SEND_FAILED;
	}
//	if(network_send(nSocket, (char*)remote_filepath, msgHeader.nDataSize) != MARC_NETWORK_OK) //��Ϣ����
	if(network_send(nSocket, (char*)remote_filepath, strlen(remote_filepath)) != MARC_NETWORK_OK)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "network_send() failed! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_SEND_FAILED;
	}

	//���ղ��������˷��ص�ȷ����Ϣ
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
	case SFTP_CMD_UPLOAD_FILE_YES: //����ɹ�����ʼ�����ļ�
		nMaxDataSize = msgHeader.nDataSize; //���������ÿ�δ������ݵ����ֵ
		assert(nMaxDataSize > 0);
		nRetCode = UploadLocalFile(nSocket, local_filepath, nFileSize, nMaxDataSize);
		break;
	case SFTP_CMD_UPLOAD_FILE_NO: //����ʧ��
		Logger.Write(CRunLogger::LOG_ERROR, "sftp server returned SFTP_CMD_UPLOAD_FILE_NO! %s:%d\n", __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_FILE_ERROR;
	default:
		Logger.Write(CRunLogger::LOG_ERROR, "Invalid command: %d, %s:%d\n", msgHeader.nCommand, __FILE__, __LINE__);
		CLOSE_SOCKET(nSocket);
		return SFTP_CODE_INVALID_COMMAND;
	};

	//�ϴ�����
	switch(nRetCode)
	{
	case SFTP_CODE_OK: //�ϴ��ɹ�
		memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
		msgHeader.nCommand = SFTP_CMD_OK;
		msgHeader.nOffset = nFileSize;
		Host2Net(msgHeader);
		network_send(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE);
		//Sleep(2000);
		CLOSE_SOCKET(nSocket);
		break;
	default: //�������ֱ�ӹر�
		CLOSE_SOCKET(nSocket);
		break;	
	}
	
	return nRetCode;
}

int Save2LocalFile(SOCKET nSocket, const char* sLocalFile, int nFileSize, int nMaxDataSize)
{
	int nRetCode = SFTP_CODE_OK;

	//����·��
	if(!CreateFilePath(sLocalFile))
	{
		Logger.Write(CRunLogger::LOG_ERROR, "Can't create path: %s, %s:%d\n", sLocalFile, __FILE__, __LINE__);
		return SFTP_CODE_FILE_ERROR;
	}

	//���������ļ�
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

	//ѭ����������, ֱ����ɻ��߷����쳣
	_stMsgHeader msgHeader;
	int nNextOffset = 0;
	while(nNextOffset < nFileSize)
	{
		//������������������Ϣ
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

		//��������ȷ����Ϣ
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
			//�������Ӧ�ò������
			assert(false);
			nRetCode = SFTP_CODE_FILE_ERROR;
			break;
		}

		//�������ص����ݿ�
		assert(msgHeader.nCommand == SFTP_CMD_DOWNLOAD_DATA_YES);
		assert(msgHeader.nOffset == nNextOffset);
		assert(msgHeader.nDataSize > 0);
		if(network_recv(nSocket, pDataBuf, msgHeader.nDataSize) != MARC_NETWORK_OK)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "network_recv() failed! %s:%d\n", __FILE__, __LINE__);
			nRetCode = SFTP_CODE_RECV_FAILED;
			break;
		}
		//printf("���յ�һ�����ݣ�nOffset = %d, DataSize = %d\n", nNextOffset, msgHeader.nDataSize);

		//�����յ������ݱ��浽�ļ�
		int nWriteBytes = (int)fwrite(pDataBuf, sizeof(char), msgHeader.nDataSize, fp);
		if(nWriteBytes != msgHeader.nDataSize)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "fwrite() exception, disk error or free space not enough? %s:%d\n", __FILE__, __LINE__);
			nRetCode = SFTP_CODE_FILE_ERROR;
			break;
		}

		//�´��������ص�����ƫ����
		nNextOffset += msgHeader.nDataSize;
	}
	free(pDataBuf);
	fclose(fp);

	//�����ز��ɹ���ɾ���ļ�
	if(nRetCode != SFTP_CODE_OK)
	{
		deleteFile(sLocalFile);
	}

	return nRetCode;
}

int UploadLocalFile(SOCKET nSocket, const char* sLocalFile, int nFileSize, int nMaxDataSize)
{
	int nRetCode = SFTP_CODE_OK;

	//�򿪸��ļ�����¼�ļ���С
	FILE *fp = fopen(sLocalFile, "rb");
	if(fp == NULL)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "Can't read the file: %s, not existed? %s:%d\n", sLocalFile, __FILE__, __LINE__);
		return SFTP_CODE_FILE_ERROR;
	}

	//�ļ����ݻ�����
	assert(nMaxDataSize > 0);
	char* pDataBuf = (char*)malloc(nMaxDataSize);
	assert(pDataBuf != NULL);
	memset(pDataBuf, 0, nMaxDataSize);

	//ѭ���������ݣ�ֱ����ϻ����쳣
	int nNextOffset = 0; //�´ζ�ȡ��ƫ��λ��
	while(nNextOffset < nFileSize)
	{
		//�ƶ��ļ�ָ�뵽ָ��ƫ��λ�ö�ȡ����
		fseek(fp, nNextOffset, SEEK_SET);
		int nDataSize = (int)fread(pDataBuf, sizeof(char), nMaxDataSize, fp);
		assert(nDataSize > 0);

		//���������ϴ�����
		_stMsgHeader msgHeader;
		memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
		msgHeader.nOffset = nNextOffset; //�ÿ��������ļ��е�ƫ����
		msgHeader.nDataSize =  nDataSize; //���ݴ�С
		msgHeader.nCommand = SFTP_CMD_UPLOAD_DATA_REQ;
		Host2Net(msgHeader);
		if(network_send(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "network_send() failed! %s:%d\n", __FILE__, __LINE__);
			nRetCode = SFTP_CODE_SEND_FAILED;
			break;
		}

		//�����ļ�����
		if(network_send(nSocket, pDataBuf, nDataSize) != MARC_NETWORK_OK)
		{
			//����ʧ�ܣ����ټ�������
			Logger.Write(CRunLogger::LOG_ERROR, "network_send() failed! %s:%d\n", __FILE__, __LINE__);
			nRetCode = SFTP_CODE_SEND_FAILED;
			break;
		}

		//���շ���˷��ص�ȷ����Ϣ��ֻ��ȷ�ϳɹ��ż������ͣ�
		memset(&msgHeader, 0, SFTP_MSG_HEADER_SIZE);
		if(network_recv(nSocket, (char*)&msgHeader, SFTP_MSG_HEADER_SIZE) != MARC_NETWORK_OK)
		{
			//����ʧ�ܣ����ټ�������
			Logger.Write(CRunLogger::LOG_ERROR, "network_recv() failed! %s:%d\n", __FILE__, __LINE__);
			nRetCode = SFTP_CODE_RECV_FAILED;
			break;
		}
		Net2Host(msgHeader);

		//����ȷ����Ϣ
		if(msgHeader.nCommand == SFTP_CMD_UPLOAD_DATA_YES)
		{
			//�´ζ�ȡ��ƫ��λ��
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
