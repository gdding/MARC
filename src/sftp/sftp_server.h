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
#ifndef _H_SFTP_SERVER_GDDING_INCLUDED_20100311
#define _H_SFTP_SERVER_GDDING_INCLUDED_20100311


/** 
 * 服务端API接口
 **/
typedef void SFTP_SVR;
typedef enum
{
	SFTPOPT_DOWNLOAD_FINISHED_FUNCTION	= 1, //设置文件下载完成后的回调函数
	SFTPOPT_UPLOAD_FINISHED_FUNCTION	= 2, //设置文件上传完成后的回调函数
	SFTPOPT_MAX_DATA_PACKET_SIZE		= 3, //每次下载或上传的数据包最大长度（缺省为4KB）
	SFTPOPT_PRIVATE_DATA				= 4, //设置私有数据(类型为void*)
	SFTPOPT_CONNECTION_TIMEOUT			= 5, //连接最长维持时间（秒，缺省为600秒）
}SFTPoption;

//回调函数定义(priv为通过SFTPOPT_PRIVATE_DATA设置的用户私有数据)
typedef void (*sftp_cb_download_finished)(void* priv, const char* file);
typedef void (*sftp_cb_upload_finished)(void* priv, const char* file);

SFTP_SVR*	sftp_server_init(const char* ip, unsigned short port, int max_conns=256);
bool		sftp_server_setopt(SFTP_SVR* h, SFTPoption opt, ...);
bool		sftp_server_start(SFTP_SVR* h);
void		sftp_server_stop(SFTP_SVR* h);
void		sftp_server_exit(SFTP_SVR* h);
int			sftp_server_active_conns(SFTP_SVR* h);


#endif //_H_SFTP_SERVER_GDDING_INCLUDED_20100311
