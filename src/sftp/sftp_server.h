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
 * �����API�ӿ�
 **/
typedef void SFTP_SVR;
typedef enum
{
	SFTPOPT_DOWNLOAD_FINISHED_FUNCTION	= 1, //�����ļ�������ɺ�Ļص�����
	SFTPOPT_UPLOAD_FINISHED_FUNCTION	= 2, //�����ļ��ϴ���ɺ�Ļص�����
	SFTPOPT_MAX_DATA_PACKET_SIZE		= 3, //ÿ�����ػ��ϴ������ݰ���󳤶ȣ�ȱʡΪ4KB��
	SFTPOPT_PRIVATE_DATA				= 4, //����˽������(����Ϊvoid*)
	SFTPOPT_CONNECTION_TIMEOUT			= 5, //�����ά��ʱ�䣨�룬ȱʡΪ600�룩
}SFTPoption;

//�ص���������(privΪͨ��SFTPOPT_PRIVATE_DATA���õ��û�˽������)
typedef void (*sftp_cb_download_finished)(void* priv, const char* file);
typedef void (*sftp_cb_upload_finished)(void* priv, const char* file);

SFTP_SVR*	sftp_server_init(const char* ip, unsigned short port, int max_conns=256);
bool		sftp_server_setopt(SFTP_SVR* h, SFTPoption opt, ...);
bool		sftp_server_start(SFTP_SVR* h);
void		sftp_server_stop(SFTP_SVR* h);
void		sftp_server_exit(SFTP_SVR* h);
int			sftp_server_active_conns(SFTP_SVR* h);


#endif //_H_SFTP_SERVER_GDDING_INCLUDED_20100311
