#include "../utils/myIniFile.h"
#include "../utils/Utility.h"
#include "TypeDefs.h"


CMasterConf::CMasterConf(const char* sConfFile, const char* sInstallPath)
{
	INITIALIZE_LOCKER(locker4ClientApp);
	INITIALIZE_LOCKER(locker4ResultApp);
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "basic", "IP", sIP)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "basic", "Port", nPort)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "httpd", "Enabled", nHttpdEnabled)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "httpd", "Port", nHttpdPort)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "update", "ClientUpdateDir", sClientUpdateDir)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "update", "ResultUpdateDir", sResultUpdateDir)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "update", "VerWatchInterval", nVerWatchInterval)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "MaxListenerCount", nMaxListenerCount)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "advanced", "TaskDir", sTaskPath)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "advanced", "ZipTaskDir", sZipTaskPath)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "TaskCreateStrategy", nTaskCreateStrategy)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "MaxTaskFetchTime", nMaxTaskFetchTime)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "MaxTaskRunTime", nMaxTaskRunTime)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "MaxSaveStateTime", nMaxSaveStateTime)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "MaxPacketSize", nMaxPacketSize)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "AppRunTimeout", nAppRunTimeout)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "AutoDeleteTaskFile", nAutoDeleteTaskFile)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "TaskFailStrategy", nTaskFailStrategy)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "TaskFailMaxRetry", nTaskFailMaxRetry)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "AutoSaveUnfinishedTask", nAutoSaveUnfinishedTask)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "SourceStatusInterval", nSourceStatusInterval)) goto _ERROR;
	if(nMaxListenerCount <= 0) nMaxListenerCount = 1;
	if(nMaxPacketSize < 4096) nMaxPacketSize = 4096;
	if(nTaskFailStrategy < 0 || nTaskFailStrategy > 2) nTaskFailStrategy = 0;
	if(nTaskFailMaxRetry < 0) nTaskFailMaxRetry = 3;

	//异步消息处理
	nAsynMsgProcessing = 1;

	this->sInstallPath = sInstallPath;
	nStartupTime = time(0);	
	NormalizePath(sClientUpdateDir);
	NormalizePath(sResultUpdateDir);
	NormalizePath(sTaskPath);
	NormalizePath(sZipTaskPath);

	//创建文件夹，防止没有
	MAKE_DIR(sClientUpdateDir.c_str());
	MAKE_DIR(sResultUpdateDir.c_str());
	MAKE_DIR(sTaskPath.c_str());
	MAKE_DIR(sZipTaskPath.c_str());
	MAKE_DIR("state");
	MAKE_DIR("log");

	return;

_ERROR:
	printf("Invalid configure file %s, please check it!\n", sConfFile);
	_exit(-1);
}

CMasterConf::~CMasterConf()
{
	LOCK(locker4ClientApp);
	map<string, CAppUpdateInfo*>::iterator it = oClientAppVersion.begin();
	for(; it != oClientAppVersion.end(); ++it)
	{
		if(it->second != NULL)
			delete it->second;
	}
	oClientAppVersion.clear();
	UNLOCK(locker4ClientApp);

	LOCK(locker4ResultApp);
	it = oResultAppVersion.begin();
	for(; it != oResultAppVersion.end(); ++it)
	{
		if(it->second != NULL)
			delete it->second;
	}
	UNLOCK(locker4ResultApp);

	DESTROY_LOCKER(locker4ClientApp);
	DESTROY_LOCKER(locker4ResultApp);
}

CResultConf::CResultConf(const char* sConfFile, const char* sInstallPath)
{
	nResultID = 0;

	//读取配置文件
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "master", "MasterIp", sMasterIP)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "master", "MasterPort", nMasterPort)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "master", "BakMasterIp", sBakMasterIP)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "master", "BakMasterPort", nBakMasterPort)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "result_server", "ListenIp", sListenIP)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "result_server","ListenPort", nListenPort)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "result_server","NatEnabled", nNatEnabled)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "result_server", "NatIp", sNatIP)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "result_server", "AppType", sAppType)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "result_server", "UploadErrorLog", nUploadErrorLog)) nUploadErrorLog = 0;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "update", "UpdateEnabled", nUpdateEnabled)) nUpdateEnabled = 0;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "update", "UpdateTargetPath", sUpdateTargetPath)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "update", "UpdateInterval", nUpdateInterval)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "update", "ZipUpdateDir", sZipUpdateDir)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "advanced", "ZipResultDir", sZipResultDir)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "advanced", "DataDir", sDataDir)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "HeartbeatInterval", nHeartbeatInterval)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "MaxPacketSize", nMaxPacketSize)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "UploadTimeout", nUploadTimeout)) nUploadTimeout = 600;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced","AppRunTimeout", nAppRunTimeout)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced","ResultFailMaxRetry", nResultFailMaxRetry)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced","AutoDeleteResultFile", nAutoDeleteResultFile)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced","AutoSaveUnfinishedResultFile", nAutoSaveUnfinishedResultFile)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "SourceStatusInterval", nSourceStatusInterval)) goto _ERROR;
	if(nMaxPacketSize < 4096) nMaxPacketSize = 4096;
	if(nResultFailMaxRetry < 0) nResultFailMaxRetry = 3;

	this->sInstallPath = sInstallPath;
	nStartupTime = time(0);	
	NormalizePath(sUpdateTargetPath);
	NormalizePath(sZipUpdateDir);
	NormalizePath(sZipResultDir);
	NormalizePath(sDataDir);

	//创建目录，防止没有
	MAKE_DIR(sUpdateTargetPath.c_str());
	MAKE_DIR(sZipUpdateDir.c_str());
	MAKE_DIR(sZipResultDir.c_str());
	MAKE_DIR(sDataDir.c_str());
	MAKE_DIR("log");

	return;

_ERROR:
	printf("Invalid configure file %s, please check it!\n", sConfFile);
	_exit(-1);
}

CClientConf::CClientConf(const char* sConfFile, const char* sProListFile, const char* sInstallPath)
{
	m_sConfFile = sConfFile;
	m_sProListFile = sProListFile;

	int iProListCount = 0;
	int i = 0;
	vector<string> result;

	//读取配置文件
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "master", "MasterIp", sMasterIP)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "master", "MasterPort", nMasterPort)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "master", "BakMasterIp", sBakMasterIP)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "master","BakMasterPort", nBakMasterPort)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "client", "ID", nClientID) || nClientID < 0) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "client", "AppCmd", sAppCmd)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "client", "AppType", sAppType) || sAppType.empty()) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "client", "ResultUploadEnabled", nResultUploadEnabled)) nResultUploadEnabled = 1;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "client", "UploadErrorLog", nUploadErrorLog)) nUploadErrorLog = 0;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "update", "UpdateEnabled", nUpdateEnabled)) nUpdateEnabled = 0;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "update", "UpdateTargetPath", sUpdateTargetPath)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "update", "UpdateInterval", nUpdateInterval)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "update", "ZipUpdateDir", sZipUpdateDir)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "advanced", "InputDir", sInputDir)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "advanced", "OutputDir", sOutputDir)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "advanced", "ZipTaskDir", sZipTaskDir)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniStr(sConfFile, "advanced", "ZipResultDir", sZipResultDir)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "HeartbeatInterval", nHeartbeatInterval)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "StateInterval", nStateInterval)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "TaskReqWaitTime", nTaskReqWaitTime)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "AppRunTimeout", nAppRunTimeout)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "AsynUpload", nAsynUpload)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "MaxWaitingUploadFiles", nMaxWaitingUploadFiles)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "AutoDeleteResultFile", nAutoDeleteResultFile)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "AutoDeleteTaskFile", nAutoDeleteTaskFile)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "RememberResultAddr", nRememberResultAddr)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "TaskReqTimeInterval", nTaskReqTimeInterval)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "AutoSaveUploadFile", nAutoSaveUploadFile)) goto _ERROR;
	if(!INI::CMyIniFile::ReadIniInt(sConfFile, "advanced", "SourceStatusInterval", nSourceStatusInterval)) goto _ERROR;
	
	//目录路径名规范化
	this->sInstallPath = sInstallPath;
	nStartupTime = time(0);	
	NormalizePath(sAppCmd, false);
	NormalizePath(sUpdateTargetPath);
	NormalizePath(sZipUpdateDir);
	NormalizePath(sInputDir);
	NormalizePath(sOutputDir);
	NormalizePath(sZipTaskDir);
	NormalizePath(sZipResultDir);

	//创建目录，防止没有
	MAKE_DIR(sUpdateTargetPath.c_str());
	MAKE_DIR(sZipUpdateDir.c_str());
	MAKE_DIR(sInputDir.c_str());
	MAKE_DIR(sOutputDir.c_str());
	MAKE_DIR(sZipTaskDir.c_str());
	MAKE_DIR(sZipResultDir.c_str());
	MAKE_DIR("state");
	MAKE_DIR("log");

	//得到sAppCmdFile
	sAppCmdFile = "";
	SplitCmdStringBySpace(sAppCmd.c_str(), result);
	if(!result.empty())
		sAppCmdFile = result[0];

	//当前应用程序版本号
	sAppVerFile = sAppType + ".marc.ver";
	nCurAppVersion = ReadAppVersion(sAppVerFile, sAppType);

	//读取配置文件得到待杀死的进程列表
	oAppProcessList.clear();
	INI::CMyIniFile::ReadIniInt(sProListFile, "info", "count", iProListCount);
	for(i = 1; i <= iProListCount; i++)
	{
		CProInfo stPI;
		char chSeg[64] = {0};
		sprintf(chSeg ,"pro%d", i);
		INI::CMyIniFile::ReadIniStr(sProListFile, chSeg, "name", stPI.sProName);
		INI::CMyIniFile::ReadIniStr(sProListFile, chSeg, "path", stPI.sProPath);
		oAppProcessList.push_back(stPI);
	}

	return;

_ERROR:
	printf("Invalid configure file %s, please check it!\n", sConfFile);
	_exit(-1);
}

CTaskStatInfo::CTaskStatInfo()
{
	nTotalCreatedTasks		= 0;
	nTotalCreatedTimeUsed	= 0;
	nLastCreatedTaskID		= 0;
	nLastCreatedTime		= 0;
	sLastCreatedTaskFile	= "";
	nTotalDeliveredTasks	= 0;
	nTotalFinishdTasks		= 0;
	nTotalFailedTasks		= 0;
	INITIALIZE_LOCKER(m_locker);
}

CTaskStatInfo::~CTaskStatInfo()
{
	DESTROY_LOCKER(m_locker);
}

CResultNodeInfo::CResultNodeInfo()
{
	this->nResultID			= 0;
	this->pResultNode		= NULL;
	this->sInstallPath		= "";
	this->nRegisterTime		= 0;
	this->nLastActiveTime	= 0;
	this->nHeartSocket		= INVALID_SOCKET;
	this->bDisabled			= false;
	this->nDisabledTime		= 0;
	memset(&this->dNodeStatus, 0, sizeof(_stResultStatus));
	memset(&this->dSourceStatus, 0, sizeof(_stNodeSourceStatus));
}

CResultNodeInfo::~CResultNodeInfo()
{
	if(this->pResultNode != NULL)
		free(this->pResultNode);
}

CClientInfo::CClientInfo()
{
	this->nClientID			= 0;
	this->sAppType			= "";
	this->sInstallPath		= "";
	this->sIP				= "";
	this->nPort				= 0;
	this->bTaskRequested	= false;
	this->nCurTaskID		= 0;
	this->nRegisterTime		= 0;
	this->nLastActiveTime	= 0;
	this->tLastUpdateTime	= 0;
	this->nHeartSocket		= INVALID_SOCKET;
	this->bDisabled			= false;
	this->nDisabledTime		= 0;
	memset(&this->dAppState, 0, sizeof(_stClientState));
	memset(&this->dNodeStatus, 0, sizeof(_stClientStatus));
	memset(&this->dSourceStatus, 0, sizeof(_stNodeSourceStatus));
}

CAppUpdateInfo::CAppUpdateInfo()
{
	this->sAppType			= "";
	this->nAppUpdateVer		= 0;
	this->sAppUpdateFile	= "";
}

void ParseAppTypeAndInstallPath(const char* buf, string& sAppType, string& sInstallPath)
{
	sAppType = "";
	sInstallPath = "";

	int i = 0;
	for(; buf[i] != 0; i++)
	{
		if(buf[i] == ':') break;
		sAppType += buf[i];
	}
	if(buf[i]==0) return;

	i++;
	for(; i < buf[i] != 0; i++)
	{
		sInstallPath += buf[i];
	}
}

bool GetSourceStatusInfo(_stNodeSourceStatus& status)
{
	memset(&status, 0, sizeof(_stNodeSourceStatus));
	status.watch_timestamp = time(0);

	//运行marc_cdmn.sh
	char cmd[100] = {0};
	sprintf(cmd, "%s", MARC_CDMN_SCRIPT_FILE);
	if(!Exec(cmd, 5)) 
	{
		printf("ERROR: can't exec : %s\n", cmd);
		return false;
	}

	FILE *fp = fopen(MARC_CDMN_OUTPUT_FILE, "rb");
	if(fp == NULL) 
	{
		printf("ERROR: can't open the file: %s\n", MARC_CDMN_OUTPUT_FILE);
		return false;
	}
	char sLine[1024];
	while (fgets(sLine, 1024, fp))
	{
		char sKey[512] = "";
		int nValue = 0;
		if(sscanf(sLine, "%s %d", sKey, &nValue) != 2) continue;
		if(sKey[0] == 0) continue;
		if(_stricmp(sKey, "cpu_idle_ratio") == 0) status.cpu_idle_ratio = nValue;
		if(_stricmp(sKey, "disk_avail_ratio") == 0) status.disk_avail_ratio = nValue;
		if(_stricmp(sKey, "memory_avail_ratio") == 0) status.memory_avail_ratio = nValue;
		if(_stricmp(sKey, "nic_bps") == 0) status.nic_bps = nValue;
	}
	fclose(fp);
	deleteFile(MARC_CDMN_OUTPUT_FILE);
	return true;
}

int ReadAppVersion(const string& sVerFile, const string& sAppType)
{
	int nAppVersion = 0;
	FILE *fp = fopen(sVerFile.c_str(), "rt");
	if(fp != NULL)
	{
		char sLine[256] = {0};
		
		while (fgets(sLine, 100, fp))
		{
			char chAppType[256] = {0};
			int nAppVer = 0;
			if(sscanf(sLine,"%s %d", chAppType, &nAppVer) == 2 && chAppType == sAppType)
			{
				nAppVersion = nAppVer;
				break;
			}
		}
		fclose(fp);
	}
	return nAppVersion;
}

bool WriteAppVersion(const string& sVerFile, const string& sAppType, int nVersion)
{
	FILE *fp = fopen(sVerFile.c_str(), "wt");
	if(fp != NULL)
	{
		fprintf(fp, "%s %d", sAppType.c_str(), nVersion);
		fclose(fp);
		return true;
	}
	return false;
}
