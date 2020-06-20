/*-----------------------------------------------------------------------------
* Copyright (c) 2010 ICT, CAS. All rights reserved.
*   dingguodong@software.ict.ac.cn
*   yangshutong@software.ict.ac.cn
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.

* Last-Modified: 2010-03-15
*-----------------------------------------------------------------------------*/
#ifndef _H_SFTPCLIENT_GDDING_INCLUDED_20100311
#define _H_SFTPCLIENT_GDDING_INCLUDED_20100311


/** 
 * 客户端API接口
 **/

//返回码定义
#define SFTP_CODE_OK								0
#define SFTP_CODE_CONNECT_FAILED					9201			//连接服务端失败
#define SFTP_CODE_SEND_FAILED						9202			//向服务端发送数据失败
#define SFTP_CODE_RECV_FAILED						9203			//从服务端接收数据失败
#define SFTP_CODE_FILE_ERROR						9204			//文件访问失败
#define SFTP_CODE_INVALID_COMMAND					9205			//非法的命令

//每上传/下载一个文件都创建新连接
int sftp_client_download_file(const char* svr_ip, unsigned short svr_port, const char* remote_filepath, const char* local_filepath);
int sftp_client_upload_file(const char* svr_ip, unsigned short svr_port, const char* local_filepath, const char* remote_filepath);

//目录的上传/下载（待实现）
int sftp_client_download_dir(const char* svr_ip, unsigned short svr_port, const char* remote_dir, const char* local_dir);
int sftp_client_upoad_dir(const char* svr_ip, unsigned short svr_port, const char* local_dir, const char* remote_dir);

//在一个连接中上传或下载多个文件（待实现）
typedef void SFTP_CLT;
SFTP_CLT* sftp_client_init(const char* svr_ip, unsigned short svr_port);
int sftp_client_download_file(SFTP_CLT* h, const char* remote_filepath, const char* local_filepath);
int sftp_client_upload_file(SFTP_CLT* h, const char* local_filepath, const char* remote_filepath);
void sftp_client_exit(SFTP_CLT* h);

#endif //_H_SFTPCLIENT_GDDING_INCLUDED_20100311
