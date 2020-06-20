/*-----------------------------------------------------------------------------
* Copyright (c) 2010~2011 ICT, CAS. All rights reserved.
*   dingguodong@ict.ac.cn OR gdding@hotmail.com
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.
*-----------------------------------------------------------------------------*/
#include "TaskManager.h"
#include "ResultNodeManager.h"
#include "MasterNode.h"
#include "ResultNode.h"
#include "ClientNode.h"
#include "ClientManager.h"
#include "HttpServer.h"
#include "../utils/Utility.h"
#include "../utils/RunLogger.h"

#ifdef _DEBUG
	#pragma comment(lib, "utilsD")
	#pragma comment(lib, "sftpD")
#else
	#pragma comment(lib, "utils")
	#pragma comment(lib, "sftp")
#endif
#pragma comment(lib, "libevent")

static const char* MARC_VERSION = "4.3.4.3267";
static const char* MARC_PIDFILE = "marc.pid";
static volatile bool g_bGotSigterm = false;
static CHttpServer* g_pHttpServer = NULL;
static volatile bool g_bDaemonize = false;

void usage(int argc, char* argv[])
{
	printf("MARC - Distributed Task Scheduling and Executing Framework\n");
	printf("Version: MARC-%s\n", MARC_VERSION);
	printf("Copyright (c) 2010-2011, Institue of Computing Technology, CAS.\n");
	printf("Author: Guodong Ding. Email: gdding@hotmail.com\n\n");
	printf("Usage: %s [master|result|client] [-d]\n", argv[0]);
	printf("where: \n"
		   "    master - startup as a master node\n"
		   "    result - startup as a result node\n"
		   "    client - startup as a client node\n"
		   "    -d     - run as a daemon\n\n");
}

void sigterm ( int )
{
	printf( "caught SIGTERM | SIGINT | SIGHUP, shutting down...\n" );
	if(g_pHttpServer != NULL)
	{
		printf("stop http server...\n");
		g_pHttpServer->Stop();
		printf("http server stopped successfully\n");
	}
	if(g_bDaemonize)
	{
		unlink(MARC_PIDFILE);
	}
	g_bGotSigterm = true;
}

void SetSignalHandlers ()
{
#ifndef WIN32
	struct sigaction sa;
	sigfillset ( &sa.sa_mask );
	sa.sa_flags = SA_NOCLDSTOP;
	for ( ;; )
	{
		sa.sa_handler = sigterm;	if ( sigaction ( SIGTERM, &sa, NULL )!=0 ) break;
		sa.sa_handler = sigterm;	if ( sigaction ( SIGINT, &sa, NULL )!=0 ) break;
		sa.sa_handler = sigterm;	if ( sigaction ( SIGHUP, &sa, NULL )!=0 ) break;
		break;
	}
#endif
}

void daemonize(void) 
{
#ifndef WIN32
	int fd;
	if (fork() != 0) exit(0); /* parent exits */
	setsid(); /* create a new session */

	/* Every output goes to /dev/null. If Redis is daemonized but
	 * the 'logfile' is set to 'stdout' in the configuration file
	 * it will not log at all. */
	if ((fd = open("/dev/null", O_RDWR, 0)) != -1) 
	{
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		if (fd > STDERR_FILENO) close(fd);
	}

	/* Try to write the pid file in a best-effort way. */
	FILE *fp = fopen(MARC_PIDFILE, "w");
	if (fp) 
	{
		fprintf(fp,"%d\n",(int)getpid());
		fclose(fp);
	}
#endif
}

//获得当前运行路径
string GetCurRunPath()
{
#ifdef WIN32
	TCHAR szPath[MAX_PATH];
	GetModuleFileName(NULL, szPath, MAX_PATH);
	return getFilePath(szPath);
#else
	char buf[4097] = {0};
	long size = pathconf(".", _PC_PATH_MAX); 
	char *ptr = (char *)malloc(size);
	assert(ptr != NULL);
	memset(ptr,0,size);
	sprintf(ptr,"/proc/%d/exe",getpid()); 
	readlink(ptr,buf,size);
	free(ptr);
	return getFilePath(buf);
	//sPath = buf;
	//int nPos = sPath.find_last_of("/");
	//sPath = sPath.substr(0,nPos);
#endif
}

void master_run(const char* sParamFile, const char* sRunPath)
{
	CRunLogger* pLogger = new CRunLogger("./log/marc_master.log", true);
	pLogger->Write(CRunLogger::LOG_INFO, "Startup MasterNode...\n");
	pLogger->Write(CRunLogger::LOG_INFO, "Current running path: %s\n", sRunPath);

	g_bGotSigterm = false;
	CMasterConf* pConfigure = new CMasterConf(sParamFile, sRunPath);

	//初始化Master节点
	CMasterNode* pMaster = new CMasterNode(pConfigure, pLogger);
	assert(pMaster != NULL);

	//启动Master节点
	if(!pMaster->Start()) goto _EXIT;
	pLogger->Write(CRunLogger::LOG_INFO, "Startup successfully\n");

	//启动HTTP服务
	if(pConfigure->nHttpdEnabled != 0)
	{
		g_pHttpServer = new CHttpServer(pMaster, pLogger);
		assert(g_pHttpServer != NULL);
		if(!g_pHttpServer->Start()) goto _STOP;	
	}

	//轮询，直到用户终止
	while(!g_bGotSigterm) Sleep(1000);

_STOP:
	//停止Master
	pLogger->Write(CRunLogger::LOG_INFO, "Stop MasterNode...\n");
	assert(pMaster != NULL);
	pMaster->Stop();

	//停止HTTP服务
	if(g_pHttpServer != NULL) g_pHttpServer->Stop();

_EXIT:
	if(g_pHttpServer != NULL) delete g_pHttpServer;
	if(pMaster != NULL) delete pMaster;
	if(pConfigure != NULL) delete pConfigure;
	pLogger->Write(CRunLogger::LOG_INFO, "Stop successfully.\n");
	if(pLogger != NULL) delete pLogger;
}

void result_run(const char* sConfFile, const char* sRunPath)
{
	CRunLogger* pLogger = new CRunLogger("./log/marc_result.log", true);
	CResultConf* pConfigure = new CResultConf(sConfFile, sRunPath);

_START:
	pLogger->Write(CRunLogger::LOG_INFO, "Startup ResultNode...\n");
	pLogger->Write(CRunLogger::LOG_INFO, "Current running path: %s\n", sRunPath);
	
	g_bGotSigterm = false;
	bool bNeedRestart = false;

	//启动，若失败则切换Master端，等待30秒重新启动
	CResultNode* pResultNode = new CResultNode(pConfigure, pLogger);
	assert(pResultNode != NULL);
	bool bUseBakMaster = false;
	while(!pResultNode->Start(bUseBakMaster))
	{
		pLogger->Write(CRunLogger::LOG_INFO, "Startup failed, switch MasterNode, waiting for 30 seconds...\n");
		bUseBakMaster = (!bUseBakMaster);
		int nSleepCount = 30;
		while(nSleepCount-- > 0 && !g_bGotSigterm) Sleep(1000);
		if(g_bGotSigterm) goto _EXIT;
	}

	pLogger->Write(CRunLogger::LOG_INFO, "Startup successfully, Listening on %s:%d\n", pConfigure->sListenIP.c_str(), pConfigure->nListenPort);

	//轮询，直到用户终止或发生错误
	while(!g_bGotSigterm)
	{
		//判断是否发生错误
		if(pResultNode->NeedRestart())
		{
			pLogger->Write(CRunLogger::LOG_WARNING, "Detected to need restart! %s:%d\n", __FILE__, __LINE__);
			bNeedRestart = true;
			break;
		}

		Sleep(1000);
	}

_EXIT:
	//终止
	pLogger->Write(CRunLogger::LOG_INFO, "Stop ResultNode...\n");
	pResultNode->Stop();
	if(pResultNode != NULL) delete pResultNode;
	pLogger->Write(CRunLogger::LOG_INFO, "Stop successfully\n");

	//若需要重启
	if(bNeedRestart && !g_bGotSigterm)
	{
		pLogger->Write(CRunLogger::LOG_INFO, "Waiting for 30 seconds to restart...\n");
		int nSleepCount = 30;
		while(nSleepCount-- > 0 && !g_bGotSigterm) Sleep(1000);
		if(!g_bGotSigterm) goto _START;
	}

	if(pConfigure != NULL) delete pConfigure;
	if(pLogger != NULL) delete pLogger;
}

void client_run(const char* sConfFile, const char* sProListFile, const char* sRunPath)
{	
	CRunLogger* pLogger = new CRunLogger("./log/marc_client.log", true);
	CClientConf* pConfigure = new CClientConf(sConfFile, sProListFile, sRunPath);

_START:
	pLogger->Write(CRunLogger::LOG_INFO, "Startup ClientNode...\n");
	pLogger->Write(CRunLogger::LOG_INFO, "Current running path: %s\n", sRunPath);

	g_bGotSigterm = false;
	bool bNeedRestart = false;

	//启动，若失败则切换Master，等待30秒重新启动
	CClientNode* pClientNode = new CClientNode(pConfigure, pLogger);
	assert(pClientNode != NULL);
	bool bUseBakMaster = false;	
	while(!pClientNode->Start(bUseBakMaster))
	{
		pLogger->Write(CRunLogger::LOG_INFO, "Startup failed, switch MasterNode, waiting for 30 seconds to restart...\n");
		bUseBakMaster = (!bUseBakMaster);
		int nSleepCount = 30;
		while(nSleepCount-- > 0 && !g_bGotSigterm) Sleep(1000);
		if(g_bGotSigterm) goto _EXIT;
	}

	pLogger->Write(CRunLogger::LOG_INFO, "Startup successfully\n");

	//轮询，直到用户终止或发生错误
	while(!g_bGotSigterm)
	{
		//是否需要重启
		if(pClientNode->NeedRestart())
		{
			pLogger->Write(CRunLogger::LOG_WARNING, "Detected to need restart! %s:%d\n", __FILE__, __LINE__);
			bNeedRestart = true;
			break;
		}

		Sleep(1000);
	}

_EXIT:
	pLogger->Write(CRunLogger::LOG_INFO, "Stop ClientNode...\n");
	pClientNode->Stop();
	if(pClientNode != NULL) delete pClientNode;
	pLogger->Write(CRunLogger::LOG_INFO, "Stop successfully\n");

	if(bNeedRestart && !g_bGotSigterm)
	{
		pLogger->Write(CRunLogger::LOG_INFO, "Waiting for 30 seconds to restart...\n");
		int nSleepCount = 30;
		while(nSleepCount-- > 0 && !g_bGotSigterm) Sleep(1000);
		if(!g_bGotSigterm) goto _START;
	}

	if(pConfigure != NULL) delete pConfigure;
	if(pLogger != NULL) delete pLogger;
}

int main(int argc, char* argv[])
{
	if(argc < 2)
	{
		usage(argc, argv);
		return 0;
	}
	if(argc > 2)
	{
		g_bDaemonize = (strcmp(argv[2], "-d") == 0);
	}

	//改变工作路径
	string sCurRunPath = GetCurRunPath();
	chdir(sCurRunPath.c_str());

	//pid文件存在则退出
	if(DIR_EXIST(MARC_PIDFILE))
	{
		printf("Warning: pid file exists: %s, maybe running or exit abnormally.\n", MARC_PIDFILE);
		return 0;
	}

	//以守护进程方式运行?
	if(g_bDaemonize) daemonize();

#ifdef _WIN32
	WORD wVersion=MAKEWORD(2,2);
	WSADATA wsData;
	if (WSAStartup(wVersion,&wsData) != 0)  return -1;
#else
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) return -1;
#endif

	//设置信号处理器
	SetSignalHandlers ();
	
	if(strcmp(argv[1], "master") == 0) //master端
	{
		master_run(MARC_MASTER_CONF_FILE, sCurRunPath.c_str());
	}
	else if(strcmp(argv[1], "client") == 0) //client端
	{
		client_run(MARC_CLIENT_CONF_FILE, MARC_KILLPROLIST_CONF_FILE, sCurRunPath.c_str());
	}
	else if(strcmp(argv[1], "result") == 0) //result端
	{
		result_run(MARC_RESULT_CONF_FILE, sCurRunPath.c_str());
	}
	else
	{
		usage(argc, argv);
	}

#ifdef _WIN32
	WSACleanup();
#endif

	return 0;
}

