/*-----------------------------------------------------------------------------
* Copyright (c) 2010~2011 ICT, CAS. All rights reserved.
*   dingguodong@software.ict.ac.cn
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.
*-----------------------------------------------------------------------------*/
#ifndef _H_NETWORK_GDDING_INCLUDED_20111012
#define _H_NETWORK_GDDING_INCLUDED_20111012
#include "StdHeader.h"

#define MARC_NETWORK_OK			0	//�ɹ�
#define MARC_NETWORK_ERROR		-1	//����
#define MARC_NETWORK_TIMEOUT	1	//��ʱ
#define MARC_NETWORK_CLOSED		2	//���ӱ��ر�


//��ʼ��TCP����socket���ɹ����ش�����socket��ʧ�ܷ���INVALID_SOCKET
SOCKET network_listener_init(const char* ip, unsigned short port, int backlog);

// ���ӷ��������ɹ��������ӵ�socket��ʧ�ܷ���INVALID_SOCKET
SOCKET network_connect(const char* pSvrIp, unsigned short ulSvrPort);

//Ϊ�˷�ֹ���������ر�д�����ݷ��ͺͽ��պ���
int network_send(SOCKET listen_socket,char* pSendBuf, int nBufLen, int nMaxTries=6, bool bEnc = true);
int network_recv(SOCKET listen_socket,char* pRecvBuf, int nBufLen, int nMaxTries=6, bool bDec = true);

bool isBigEndian();
unsigned long long htonll(unsigned long long x);
unsigned long long ntohll(unsigned long long x);

//��buf�н�����IP��PORT��buf��ʽΪIP:PORT
bool GetIPandPort(const char* buf, string& ip, unsigned short& port);


#endif //_H_NETWORK_GDDING_INCLUDED_20111012
