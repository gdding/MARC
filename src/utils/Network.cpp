#include "Network.h"
#include "RunLogger.h"

//加密解密函数
static void marc_enc(char *pData, int nSize);
static void marc_dec(char *pData, int nSize);
static void encrypt(char* pData, int nSize, const char* pKey, int nKeyLen);
static CRunLogger Logger("./log/marc_network.log", true);

SOCKET network_listener_init(const char* ip, unsigned short port, int backlog)
{
	SOCKET nSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_IP);
	if (nSocket != INVALID_SOCKET)
	{
		int opt = 1;
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = inet_addr(ip);
		if(setsockopt(nSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) == SOCKET_ERROR) 
		{ 
			CLOSE_SOCKET(nSocket);
			return INVALID_SOCKET;
		}
		if (bind(nSocket, (sockaddr*)&addr, sizeof(sockaddr)) == SOCKET_ERROR)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "bind error: %s, %s:%d\n", strerror(errno), __FILE__, __LINE__);
			CLOSE_SOCKET(nSocket);
			return INVALID_SOCKET;
		}
		if (listen(nSocket, backlog) == SOCKET_ERROR)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "bind error: %s, %s:%d\n", strerror(errno), __FILE__, __LINE__);
			CLOSE_SOCKET(nSocket);
			return INVALID_SOCKET;
		}
	}
	return nSocket;
}


SOCKET network_connect(const char* pSvrIp, unsigned short ulSvrPort)
{
	int nClientSock = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if(nClientSock != INVALID_SOCKET)
	{
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(ulSvrPort);
		addr.sin_addr.s_addr = inet_addr(pSvrIp);
		if(connect(nClientSock, (sockaddr*)&addr, sizeof(sockaddr)) == SOCKET_ERROR)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "connect error: %s, %s:%d\n", strerror(errno), __FILE__, __LINE__);
			CLOSE_SOCKET(nClientSock);
			nClientSock = INVALID_SOCKET;
		}
	}
	return nClientSock;
}

// 返回值: 0 成功, < 0 失败, 1 超时
int network_send(SOCKET listen_socket, char* pSendBuf, int nBufLen, int nMaxTries, bool bEnc)
{
	if(listen_socket == INVALID_SOCKET) return MARC_NETWORK_ERROR;
	if( nBufLen <= 0 ) return MARC_NETWORK_OK;

	//对发送的数据进行加密
	char* pEncBuf = NULL;
	if(bEnc)
	{
		pEncBuf = (char*)malloc(nBufLen);
		assert(pEncBuf != 0);
		memcpy(pEncBuf, pSendBuf, nBufLen);
		marc_enc(pEncBuf, nBufLen);
	}
	else
	{
		pEncBuf = pSendBuf;
	}

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(listen_socket, &fds);

	int nTotalSendLen = 0; //已发送的数据长度
	int nRetCode = 0; //返回码
	int nRetryCount = 0; //重试次数
	while(nTotalSendLen < nBufLen)
	{
		struct timeval timeout;
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;
		int nSelect = select((int)listen_socket+1, NULL, &fds, NULL, &timeout);
		if( nSelect < 0 ) //失败
		{
			Logger.Write(CRunLogger::LOG_ERROR, "select error: %s, %s:%d\n", strerror(errno), __FILE__, __LINE__);
			nRetCode = MARC_NETWORK_ERROR; 
			break;
		}
		else if( nSelect == 0) //超时
		{
			if(++nRetryCount > nMaxTries)
			{
				Logger.Write(CRunLogger::LOG_ERROR, "network_send timeout! %s:%d\n", __FILE__, __LINE__);
				nRetCode = MARC_NETWORK_TIMEOUT;
				break;
			}
			continue;
		}

#ifdef _WIN32
		int nSendLen = send(listen_socket, &pEncBuf[nTotalSendLen], nBufLen - nTotalSendLen, 0);
		if(nSendLen <= 0)  //失败
		{
			Logger.Write(CRunLogger::LOG_ERROR, "send error: %s, %s:%d\n", strerror(errno), __FILE__, __LINE__);
			nRetCode = MARC_NETWORK_ERROR; 
			break;
		}
#else
		int nSendLen = send(listen_socket, &pEncBuf[nTotalSendLen], nBufLen - nTotalSendLen, MSG_NOSIGNAL);
		if(nSendLen<0 && errno==EPIPE) //失败
		{
			Logger.Write(CRunLogger::LOG_ERROR, "send error: %s, %s:%d\n", strerror(errno), __FILE__, __LINE__);
			nRetCode = MARC_NETWORK_ERROR; 
			break;
		}
#endif

		nTotalSendLen += nSendLen;
	}
	FD_CLR(listen_socket, &fds);
	if(bEnc) free(pEncBuf);

	return nRetCode;
}

// 返回值: 0 成功, < 0 失败, 1 超时，2 连接关闭
int network_recv(SOCKET listen_socket, char* pRecvBuf, int nBufLen, int nMaxTries, bool bDec)
{
	if(listen_socket == INVALID_SOCKET) return MARC_NETWORK_ERROR;
	if( nBufLen == 0 ) return MARC_NETWORK_OK;
	
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(listen_socket, &fds);

	int nTotalRecvLen = 0; //已接收的数据总长度
	int nRetCode = 0; //返回码
	int nRetryCount = 0; //重试次数
	while(nTotalRecvLen < nBufLen)
	{
		struct timeval timeout;
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;
		int nSelect = select((int)listen_socket+1, &fds, NULL, NULL, &timeout);
		if( nSelect < 0 ) //失败
		{ 
			Logger.Write(CRunLogger::LOG_ERROR, "select error: %s! %s:%d\n", strerror(errno), __FILE__, __LINE__);
			nRetCode = MARC_NETWORK_ERROR;
			break;
		}
		else if( nSelect == 0 ) //超时
		{
			if(++nRetryCount > nMaxTries)
			{
				Logger.Write(CRunLogger::LOG_ERROR, "network_recv timeout! %s:%d\n", __FILE__, __LINE__);
				nRetCode = MARC_NETWORK_TIMEOUT; 
				break;
			}
			continue;
		}

		int nRecvLen = recv(listen_socket, &pRecvBuf[nTotalRecvLen], nBufLen - nTotalRecvLen, 0);
		if (nRecvLen < 0) 
		{
			Logger.Write(CRunLogger::LOG_ERROR, "recv error: %s, %s:%d\n", strerror(errno), __FILE__, __LINE__);
			nRetCode = MARC_NETWORK_ERROR;
			break;
		}
		else if(nRecvLen == 0 ) //连接关闭
		{
			Logger.Write(CRunLogger::LOG_ERROR, "connection closed! %s:%d\n", __FILE__, __LINE__);
			nRetCode = MARC_NETWORK_CLOSED; 
			break;
		}

		nTotalRecvLen += nRecvLen;
	}
	FD_CLR(listen_socket, &fds);

	if(bDec)
	{
		//添加解密处理
		marc_dec(pRecvBuf, nBufLen);
	}

	return nRetCode;
}

static const char* MARC_KEY = "0123456789abcdefghijklmnopqrstuvwxyz";
static const int MARC_KEYLEN = 36;

void marc_enc(char *pData, int nSize)
{
	encrypt(pData, nSize, MARC_KEY, MARC_KEYLEN);
}

void marc_dec(char *pData, int nSize)
{
	encrypt(pData, nSize, MARC_KEY, MARC_KEYLEN);
}

void encrypt(char* pData, int nDataLen, const char* pKey, int nKeyLen)
{
	const static unsigned long long KEYKEY = htonll(2713651036219876540LL);
	if(pKey != NULL && nKeyLen > 0)
	{
		char* pKeyKey = (char*)malloc(nKeyLen);
		assert(pKeyKey != NULL);
		memcpy(pKeyKey, pKey, nKeyLen);

		int i=0, j=0;
		while(i < nKeyLen)
		{
			pKeyKey[i] = pKeyKey[i] ^ ((unsigned char*)&KEYKEY)[j];
			i++; j++;
			if(j == sizeof(KEYKEY)) j = 0;
		}

		i=0; j=0;
		while(i < nDataLen)
		{
			pData[i] = pData[i]^pKeyKey[j]^*((char*)&i);
			i++; j++;
			if(j == nKeyLen) j = 0;
		}
		free(pKeyKey);
	}
}

bool isBigEndian()
{
	unsigned short s = 0x1234;
	return (htons(s) == s);
}

// htonll, ntohll的实现
unsigned long long htonll(unsigned long long x)
{
	// 首先判断是否是big-endian，网络字节序是big-endian
	if(isBigEndian())
		return x;

	unsigned long long y;
	unsigned int a = x >> 32;
	unsigned int b = x & 0xFFFFFFFFLL;

	a = htonl(a);
	b = htonl(b);
	y = b;
	y <<= 32;
	y |= a;
	return y;
}

unsigned long long ntohll(unsigned long long x)
{
	if(isBigEndian())
		return x;

	unsigned long long y;
	unsigned int a = x >> 32;
	unsigned int b = x & 0xFFFFFFFFLL;

	a = ntohl(a);
	b = ntohl(b);
	y = b;
	y <<= 32;
	y |= a;
	return y;
}


//从buf中解析出IP和PORT，buf格式为IP:PORT
bool GetIPandPort(const char* buf, string& ip, unsigned short& port)
{
	ip = "";
	int i = 0;
	for(; buf[i] != 0; i++)
	{
		if(buf[i] == ':') break;
		ip += buf[i];
	}
	if(buf[i]==0) return false;
	i++;
	string sPort("");
	for(; i < buf[i] != 0; i++)
	{
		sPort += buf[i];
	}
	port  = atoi(sPort.c_str());
	return true;
}
