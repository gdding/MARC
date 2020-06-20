/*-----------------------------------------------------------------------------
* Copyright (c) 2010 ICT, CAS. All rights reserved.
*   dingguodong@software.ict.ac.cn
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.

* Last-Modified: 2010-03-14
*-----------------------------------------------------------------------------*/
#ifndef _H_SFTPTYPES_GDDING_INCLUDED_20100314
#define _H_SFTPTYPES_GDDING_INCLUDED_20100314
#include "../utils/StdHeader.h"

#define SFTP_CMD_DOWNLOAD_FILE_REQ						6000			//�ͻ������������ļ�
#define SFTP_CMD_DOWNLOAD_FILE_YES						6001			//�ͻ������������ļ��ɹ�
#define SFTP_CMD_DOWNLOAD_FILE_NO						6002			//�������ص��ļ������ڣ�fopenʧ�ܣ�
#define SFTP_CMD_DOWNLOAD_DATA_REQ						6003			//�ͻ��������������ݿ�
#define SFTP_CMD_DOWNLOAD_DATA_YES						6004			//�����ȷ�����ݿ����سɹ�
#define SFTP_CMD_DOWNLOAD_DATA_NO						6005			//�����ȷ�����ݿ�����ʧ��
#define SFTP_CMD_UPLOAD_FILE_REQ						6010			//�ͻ��������ϴ��ļ�
#define SFTP_CMD_UPLOAD_FILE_YES						6011			//�ͻ��������ϴ��ļ��ɹ�
#define SFTP_CMD_UPLOAD_FILE_NO							6012			//�ͻ��������ϴ��ļ�ʧ��
#define SFTP_CMD_UPLOAD_DATA_REQ						6013			//�ͻ��������ϴ����ݿ�
#define SFTP_CMD_UPLOAD_DATA_YES						6014			//�����ȷ�����ݿ��ϴ��ɹ�
#define SFTP_CMD_UPLOAD_DATA_NO							6015			//�����ȷ�����ݿ��ϴ�ʧ��
#define SFTP_CMD_OK										6020			//���ݴ������

//���ݰ��ṹ����
typedef struct
{
	int nCommand;		//����
	int nOffset;		//ƫ������ĳЩʱ����������;��
	int nDataSize;		//������
}_stMsgHeader;
#define SFTP_MSG_HEADER_SIZE (sizeof(_stMsgHeader))


////////////////////////////////////////////////////////
//�������ֽ���תΪ�����ֽ���
inline void Net2Host(_stMsgHeader &msg)
{
	msg.nCommand = ntohl(msg.nCommand);
	msg.nDataSize = ntohl(msg.nDataSize);
	msg.nOffset = ntohl(msg.nOffset);
}

//�����ֽ���תΪ�����ֽ��� 
inline void Host2Net(_stMsgHeader &msg)
{
	msg.nCommand = htonl(msg.nCommand);
	msg.nDataSize = htonl(msg.nDataSize);
	msg.nOffset = htonl(msg.nOffset);
}
////////////////////////////////////////////////////////


#endif //_H_SFTPTYPES_GDDING_INCLUDED_20100314
