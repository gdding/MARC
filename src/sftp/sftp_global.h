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

#define SFTP_CMD_DOWNLOAD_FILE_REQ						6000			//客户端请求下载文件
#define SFTP_CMD_DOWNLOAD_FILE_YES						6001			//客户端请求下载文件成功
#define SFTP_CMD_DOWNLOAD_FILE_NO						6002			//请求下载的文件不存在（fopen失败）
#define SFTP_CMD_DOWNLOAD_DATA_REQ						6003			//客户端请求下载数据块
#define SFTP_CMD_DOWNLOAD_DATA_YES						6004			//服务端确认数据块下载成功
#define SFTP_CMD_DOWNLOAD_DATA_NO						6005			//服务端确认数据块下载失败
#define SFTP_CMD_UPLOAD_FILE_REQ						6010			//客户端请求上传文件
#define SFTP_CMD_UPLOAD_FILE_YES						6011			//客户端请求上传文件成功
#define SFTP_CMD_UPLOAD_FILE_NO							6012			//客户端请求上传文件失败
#define SFTP_CMD_UPLOAD_DATA_REQ						6013			//客户端请求上传数据块
#define SFTP_CMD_UPLOAD_DATA_YES						6014			//服务端确认数据块上传成功
#define SFTP_CMD_UPLOAD_DATA_NO							6015			//服务端确认数据块上传失败
#define SFTP_CMD_OK										6020			//数据传输完成

//数据包结构定义
typedef struct
{
	int nCommand;		//命令
	int nOffset;		//偏移量（某些时候有特殊用途）
	int nDataSize;		//包长度
}_stMsgHeader;
#define SFTP_MSG_HEADER_SIZE (sizeof(_stMsgHeader))


////////////////////////////////////////////////////////
//将网络字节序转为主机字节序
inline void Net2Host(_stMsgHeader &msg)
{
	msg.nCommand = ntohl(msg.nCommand);
	msg.nDataSize = ntohl(msg.nDataSize);
	msg.nOffset = ntohl(msg.nOffset);
}

//主机字节序转为网络字节序 
inline void Host2Net(_stMsgHeader &msg)
{
	msg.nCommand = htonl(msg.nCommand);
	msg.nDataSize = htonl(msg.nDataSize);
	msg.nOffset = htonl(msg.nOffset);
}
////////////////////////////////////////////////////////


#endif //_H_SFTPTYPES_GDDING_INCLUDED_20100314
