/*-----------------------------------------------------------------------------
* Copyright (c) 2010~2011 ICT, CAS. All rights reserved.
*   dingguodong@ict.ac.cn OR gdding@hotmail.com
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.
*-----------------------------------------------------------------------------*/
#ifndef _H_TYPEDEFS_GDDING_INCLUDED_20100204
#define _H_TYPEDEFS_GDDING_INCLUDED_20100204
#include "../utils/StdHeader.h"


//系统所需要的参数
#define MARC_MAX_MSG_BYTES							512				//512Byte
#define MARC_MAX_STATE_SIZE							500				//节点状态信息最大长度
#define MARC_MAX_CONN_COUNT							256				//最大连接数
#define MARC_HEART_TIMEOUT							300				//心跳检测超时（秒）
#define MARC_SHORT_CONN_TIMEOUT						300				//短连接生命周期（秒）
#define MARC_TASKCREATE_TIME_INTERVAL				60				//任务创建失败时隔多长时间再创建（秒）
#define MARC_TASKWATCH_TIME_INTERVAL				5				//任务执行情况监控时间间隔（秒）
#define MARC_TASKFINISHED_KEEP_TIME					3600			//已完成的任务其信息在内存中保留时间（秒）
#define MARC_TASKFINISHED_KEEP_SIZE					20				//已完成的任务其信息在内存中保留的个数
#define MARC_TASKIGNORED_KEEP_TIME					(24*3600)		//已抛弃的任务其信息在内存中保留时间（秒）
#define MARC_MAX_KEEP_LOGINFO						100				//内存中最多保留的日志数目
#define MARC_MAGIC_NUMBER							0x0123			//系统特殊魔数

//临时文件
#define MARC_REUPLOAD_RESULT_LISTFILE				"marc_reupload.result"		//重传结果文件列表
#define MARC_UNFINISHED_TASK_LISTFILE				"marc_unfinished.task"		//未完成任务文件列表
#define MARC_UNFINISHED_RESULT_LISTFILE				"marc_unfinished.result"	//未完成结果文件列表

//配置文件
#define MARC_MASTER_CONF_FILE						"master.ini"
#define MARC_RESULT_CONF_FILE						"result.ini"
#define MARC_CLIENT_CONF_FILE						"client.ini"
#define MARC_KILLPROLIST_CONF_FILE					"killprolist.ini"

//进程杀死脚本
#ifdef _WIN32
	#define MARC_KILLPROLIST_SCRIPT_FILE			"killpro.bat"
#else
	#define MARC_KILLPROLIST_SCRIPT_FILE			"./killpro.sh"
#endif

//获取资源使用状况的脚本(CPU/DISK/MEMORY/NIC)
#ifdef _WIN32
	#define MARC_CDMN_SCRIPT_FILE					"marc_cdmn.bat"
	#define MARC_CDMN_OUTPUT_FILE					"marc_cdmn.txt"
#else
	#define MARC_CDMN_SCRIPT_FILE					"./marc_cdmn.sh"
	#define MARC_CDMN_OUTPUT_FILE					"marc_cdmn.txt"
#endif

//文件/目录压缩命令
#ifdef WIN32
	#define MARC_MYZIP_APPCMD "myzip.exe"
#else
	#define MARC_MYZIP_APPCMD "./myzip"
#endif

/**
* CClientNode, CResultNode与CMasterNode之间的通讯命令
* C2M: CClientNode --> CMasterNode
* M2C: CMasterNode --> CClientNode
* R2M: CResultNode --> CMasterNode
* M2R: CMasterNode --> CResultNode
**/
#define C2M_CMD_REGISTER_REQ						4000        //Client节点向Master节点请求注册
#define M2C_CMD_REGISTER_YES						4001        //Master节点告诉Client节点注册成功
#define M2C_CMD_REGISTER_NO							4002        //Master节点告诉Client节点不允许注册
#define R2M_CMD_REGISTER_REQ						4010        //Result节点向Master请求注册
#define M2R_CMD_REGISTER_YES						4011        //Master节点告诉Result节点注册成功
#define M2R_CMD_REGISTER_NO							4012        //Master节点告诉Result节点不允许注册
#define R2M_CMD_UNREGISTER							4013		//Result节点注销
#define R2M_CMD_INSTALL_PATH						4014		//Result节点告诉Master其部署路径
#define R2M_CMD_HEART_SEND							4020        //Result节点发送心跳命令
#define R2M_CMD_SOURCE_STATUS						4021		//Result节点发送资源使用状况
#define R2M_CMD_ERRLOG_INFO							4022		//Result节点发送异常日志
#define R2M_CMD_APPVER_REQ							4030	    //Client节点请求版本核查
#define M2R_CMD_APPVER_YES							4031        //Client节点存在更新版本
#define M2R_CMD_APPVER_NO							4032        //Client节点不存在更新版本
#define R2M_CMD_CLOSE								4040		//Result节点请求关闭连接
#define C2M_CMD_CLOSE								4041		//Client节点请求关闭连接

/**
* CClientNode与CMasterListener之间的通讯命令
* C2L: CClientNode -->CMasterListener
* L2C: CMasterListener->CClientNode
**/
#define C2L_CMD_REGISTER_REQ						5000		//Client节点请求注册
#define L2C_CMD_REGISTER_YES						5001        //告诉Client节点注册成功
#define L2C_CMD_REGISTER_NO							5002        //告诉Client节点不能注册
#define C2L_CMD_UNREGISTER							5003		//Client节点注销
#define C2L_CMD_TASK_REQ							5010	    //Client节点请求任务
#define L2C_CMD_TASK_YES							5011        //告诉Client节点有任务
#define L2C_CMD_TASK_NO								5012        //告诉Client节点没有任务
#define C2L_CMD_STATE_SEND							5013		//Client节点发送状态
#define C2L_CMD_HEART_SEND							5014		//Client节点发送心跳信息
#define C2L_CMD_APP_FINISHED						5015		//Client节点程序运行正常结束
#define C2L_CMD_APP_TIMEOUT							5016		//Client节点程序运行超时
#define C2L_CMD_APP_FAILED							5017		//Client节点程序运行失败
#define C2L_CMD_TASKDOWN_FAILED						5018		//任务下载失败
#define C2L_CMD_CLOSE								5019		//Client节点请求关闭连接
#define C2L_CMD_UPLOAD_REQ							5020		//Client节点请求上传文件
#define L2C_CMD_UPLOAD_YES							5021        //Client节点可以上传文件
#define L2C_CMD_UPLOAD_NO							5022        //Client节点不能上传文件
#define C2L_CMD_TASKDOWN_SUCCESS					5023		//Client节点任务下载成功
#define C2L_CMD_SOURCE_STATUS						5024		//Client节点发送资源使用状况
#define C2L_CMD_ERRLOG_INFO							5025		//Client节点发送异常日志
#define C2L_CMD_APPVER_REQ							5030	    //Client节点请求版本核查
#define L2C_CMD_APPVER_YES							5031        //Client节点存在更新版本
#define L2C_CMD_APPVER_NO							5032        //Client节点不存在更新版本
#define L2C_CMD_INVALID_CLIENT						5555		//告知client节点其无效

//返回码定义
#define MARC_CODE_OK								0			//成功
#define MARC_CODE_INVALID_COMMAND					9002		//非法的命令
#define MARC_CODE_SELECT_FAILED						9003		//select发生错误
#define MARC_CODE_SOCKET_EXCEPTION					9004		//socket发生异常
#define MARC_CODE_CONNECT_FAILED					9101		//连接Master节点失败
#define MARC_CODE_SEND_FAILED						9102		//向Master节点发送数据失败
#define MARC_CODE_RECV_FAILED						9103		//从Master节点接收数据失败
#define MARC_CODE_REGISTER_REFUSED					9104		//Master节点拒绝注册
#define MARC_CODE_NOTASK							9105		//Master节点没有任务
#define MARC_CODE_NO_RESULT_SERVER					9106		//没有可供上传的Result节点
#define MARC_CODE_DOWNLOAD_FILE_FAILED				9107		//文件下载失败
#define MARC_CODE_UPLOAD_FILE_FAILED				9108		//文件上传失败
#define MARC_CODE_INVALID_CLIENT					9109		//client无效

//任务生成机制
#define MARC_TASKCREATE_WHEN_FREE					0			//只要Client无待处理任务就生成新任务
#define MARC_TASKCREATE_ONLYWHEN_REQUESTED			1			//当且仅当Client请求了任务且当前无待处理任务才生成新任务

//失败任务的处理策略nTaskFailStrategy
#define MARC_FAILEDTASK_STRATEGY_IGNORE				0			//抛弃,不再处理
#define MARC_FAILEDTASK_STRATEGY_KEEP				1			//只能被同一个Client节点重新执行该任务
#define MARC_FAILEDTASK_STRATEGY_AJUST				2			//允许同一类型的其他Client节点重新执行该任务

typedef enum
{
	SHORT_CONN_DEFAULT		= 0,		//其他短连接
	SHORT_CONN_TASKREQ		= 1,		//任务请求连接（短连接）
	SHORT_CONN_UPLOADREQ	= 2,		//结果回传请求连接（短连接）
	LONG_CONN_HEART			= 100,		//心跳连接（长连接）
}TConnType;

//格式化输出到HTML
typedef enum
{
	DUMP_MASTER_NODE_INFO	= 1,		//输出Master节点信息
	DUMP_RESULT_NODE_INFO	= 2,		//输出Result节点信息
	DUMP_CLIENT_NODE_INFO	= 3,		//输出Client节点信息
	DUMP_TASK_INFO			= 4,		//输出任务执行状态信息
	DUMP_MASTER_LOG_INFO	= 5,		//输出Master日志信息
	DUMP_MASTER_ERRLOG_INFO	= 6,		//输出Master异常日志信息
	DUMP_RESULT_LOG_INFO	= 7,		//输出Master日志信息
	DUMP_RESULT_ERRLOG_INFO	= 8,		//输出Master异常日志信息
	DUMP_CLIENT_LOG_INFO	= 9,		//输出Master日志信息
	DUMP_CLIENT_ERRLOG_INFO	= 10,		//输出Master异常日志信息
}TDumpInfoType;

//客户端连接信息
typedef struct
{
	SOCKET			nSocket;			//为INVALID_SOCKET时表示该连接已断开
	TConnType		nConnType;			//连接类型
	string			sIP;				//客户端IP地址
	unsigned short	nPort;				//客户端端口
	int				nClientID;			//客户端ID
	int				nStartTime;			//连接建立时间
	int				nLastActiveTime;	//最近一次活动时间
}TClientConn;

/////////////////////////////////////////////////////
//消息包结构定义
typedef struct
{
	int				nCommand;			//命令
	int				nClientID;			//节点ID
	int				nOffset;			//偏移量（某些时候有特殊用途）
	int				nBufSize;			//包长度
	char			cBuffer[MARC_MAX_MSG_BYTES+1];	//包内容
}_stDataPacket;

// 网络字节序转为主机字节序
inline void Net2Host(_stDataPacket &packet)
{
	packet.nCommand 	= ntohl(packet.nCommand);
	packet.nClientID 	= ntohl(packet.nClientID);
	packet.nOffset 		= ntohl(packet.nOffset);
	packet.nBufSize 	= ntohl(packet.nBufSize);
}

// 主机字节序转为网络字节序
inline void Host2Net(_stDataPacket &packet)
{
	packet.nCommand 	= htonl(packet.nCommand);
	packet.nClientID 	= htonl(packet.nClientID);
	packet.nOffset 		= htonl(packet.nOffset);
	packet.nBufSize 	= htonl(packet.nBufSize);
}
//////////////////////////////////////////////////////

///////////////////////////////////////////////////////
//Client节点状态信息(总长度不能超过MARC_MAX_MSG_BYTES)
typedef struct
{
	int nBufSize;	 //状态信息长度
	char cBuffer[MARC_MAX_STATE_SIZE]; //状态信息内容
} _stClientState;

inline void Net2Host(_stClientState &cs)
{
	cs.nBufSize = ntohl(cs.nBufSize);
}

inline void Host2Net(_stClientState &cs)
{
	cs.nBufSize = htonl(cs.nBufSize);
}
///////////////////////////////////////////////////////

///////////////////////////////////////////////////////
//任务下载请求信息(总长度不能超过MARC_MAX_MSG_BYTES)
typedef struct 
{
	unsigned short usPort;	//任务下载服务端口
	char chTaskFile[256];	//任务文件名
	int nTaskID;			//任务ID
}_stTaskReqInfo;

inline void Net2Host(_stTaskReqInfo &taskReq)
{
	taskReq.usPort = ntohs(taskReq.usPort);
	taskReq.nTaskID = ntohl(taskReq.nTaskID);
}

inline void Host2Net(_stTaskReqInfo &taskReq)
{
	taskReq.usPort = htons(taskReq.usPort);
	taskReq.nTaskID = htonl(taskReq.nTaskID);
}
///////////////////////////////////////////////////////

///////////////////////////////////////////////////////
//App版本信息(总长度不能超过MARC_MAX_MSG_BYTES)
typedef struct
{
	unsigned short usPort;	//版本下载服务端口
	char chUpdateFile[256];	//版本升级包文件名
	int nUpdateVersion;		//升级版本号
}_stAppVerInfo;

inline void Net2Host(_stAppVerInfo &dAppVerInfo)
{
	dAppVerInfo.usPort 			= ntohs(dAppVerInfo.usPort);
	dAppVerInfo.nUpdateVersion 	= ntohl(dAppVerInfo.nUpdateVersion);
}

inline void Host2Net(_stAppVerInfo &dAppVerInfo)
{
	dAppVerInfo.usPort 			= htons(dAppVerInfo.usPort);
	dAppVerInfo.nUpdateVersion 	= htonl(dAppVerInfo.nUpdateVersion);
}

///////////////////////////////////////////////////////

///////////////////////////////////////////////////////
//Result节点地址(总长度不能超过MARC_MAX_MSG_BYTES)
typedef struct 
{
	char chIp[16];			//IP地址
	unsigned short usPort;	//端口
	char chSavePath[256];	//存储路径
}_stResultNodeAddr;

inline void Net2Host(_stResultNodeAddr &rsAddr)
{
	rsAddr.usPort = ntohs(rsAddr.usPort);
}

inline void Host2Net(_stResultNodeAddr &rsAddr)
{
	rsAddr.usPort = htons(rsAddr.usPort);
}
///////////////////////////////////////////////////////

///////////////////////////////////////////////////////
//Result节点注册信息(总长度不能超过MARC_MAX_MSG_BYTES)
typedef struct 
{
	char chIp[16];
	int iPort;
	char chAppType[128];
	char chSavePath[256];
}_stResultNode;

inline void Net2Host(_stResultNode &rsInfo)
{
	rsInfo.iPort = ntohl(rsInfo.iPort);
}

inline void Host2Net(_stResultNode &rsInfo)
{
	rsInfo.iPort = htonl(rsInfo.iPort);
}

//节点资源使用状态信息
typedef struct
{
	unsigned int	cpu_idle_ratio;		//CPU空闲百分比
	unsigned int	disk_avail_ratio;	//可用磁盘百分比
	unsigned int	memory_avail_ratio; //可用内存百分比
	unsigned int	nic_bps;			//网卡流量（每秒字节数）
	unsigned int	watch_timestamp;	//获取时间戳
}_stNodeSourceStatus;

inline void Net2Host(_stNodeSourceStatus &status)
{
	status.cpu_idle_ratio 		= ntohl(status.cpu_idle_ratio);
	status.disk_avail_ratio 	= ntohl(status.disk_avail_ratio);
	status.memory_avail_ratio 	= ntohl(status.memory_avail_ratio);
	status.nic_bps 				= ntohl(status.nic_bps);
	status.watch_timestamp		= ntohl(status.watch_timestamp);
}

inline void Host2Net(_stNodeSourceStatus &status)
{
	status.cpu_idle_ratio 		= htonl(status.cpu_idle_ratio);
	status.disk_avail_ratio 	= htonl(status.disk_avail_ratio);
	status.memory_avail_ratio 	= htonl(status.memory_avail_ratio);
	status.nic_bps 				= htonl(status.nic_bps);
	status.watch_timestamp		= htonl(status.watch_timestamp);
}

//Result节点状态信息(总长度不能超过MARC_MAX_MSG_BYTES)
typedef struct
{
	unsigned int	nStartupTime;		//节点启动时间
	unsigned int	nOverload;			//负载（待处理结果数）
	unsigned int	nTotalReceived;		//自启动以来总共接收的结果数
	unsigned int	nTotalFinished;		//自启动以来总共成功处理的结果数
	unsigned int	nTotalTimeUsed;		//自启动以来成功处理结果文件所费的时间（秒）
	unsigned int	nTotalFailed;		//自启动以来总共结果处理失败的次数
	unsigned int	nTotalAbandoned;	//自启动以来总共抛弃不处理的结果数
	unsigned int	nLastReceivedTime;	//最近一次结果接收完成的时间
}_stResultStatus;

inline void Net2Host(_stResultStatus &status)
{
	status.nStartupTime		= ntohl(status.nStartupTime);
	status.nOverload 		= ntohl(status.nOverload);
	status.nTotalReceived 	= ntohl(status.nTotalReceived);
	status.nTotalFinished 	= ntohl(status.nTotalFinished);
	status.nTotalTimeUsed 	= ntohl(status.nTotalTimeUsed);
	status.nTotalFailed 	= ntohl(status.nTotalFailed);
	status.nTotalAbandoned 	= ntohl(status.nTotalAbandoned);
	status.nLastReceivedTime = ntohl(status.nLastReceivedTime);
}

inline void Host2Net(_stResultStatus &status)
{
	status.nStartupTime		= htonl(status.nStartupTime);
	status.nOverload 		= htonl(status.nOverload);
	status.nTotalReceived 	= htonl(status.nTotalReceived);
	status.nTotalFinished 	= htonl(status.nTotalFinished);
	status.nTotalTimeUsed 	= htonl(status.nTotalTimeUsed);
	status.nTotalFailed 	= htonl(status.nTotalFailed);
	status.nTotalAbandoned 	= htonl(status.nTotalAbandoned);
	status.nLastReceivedTime = htonl(status.nLastReceivedTime);
}

//Client节点运行状态信息（总长度不能超过MARC_MAX_MSG_BYTES）
typedef struct
{
	unsigned int	nStartupTime;			//节点启动时间
	unsigned int	nTotalFetchedTasks;		//成功下载的任务数
	unsigned int	nTotalFecthedTimeUsed;	//任务下载的总耗时（秒）
	unsigned int	nTotalFinishedTasks;	//执行成功的任务数
	unsigned int	nTotalFailedTasks;		//执行失败的任务数
	unsigned int	nTotalExecTimeUsed;		//任务执行总耗时（秒）
	unsigned int	nTotalFinishedResults;	//上传成功的结果数
	unsigned int	nTotalFailedResults;	//上传失败的结果数
	unsigned int	nTotalWaitingResults;	//当前待上传的结果数
	unsigned int	nTotalUploadedTimeUsed;	//结果上传成功的总耗时（秒）
	unsigned int	nCurTaskID;				//当前正执行的任务ID（若为0则表示节点空闲）
	unsigned int	nLastFetchedTime;		//最近一次任务下载完成的时间
	char			sLastFetchedFile[256];	//最近一次下载成功的任务文件名
}_stClientStatus;

inline void Net2Host(_stClientStatus& status)
{
	status.nStartupTime				= ntohl(status.nStartupTime);
	status.nTotalFetchedTasks 		= ntohl(status.nTotalFetchedTasks);
	status.nTotalFecthedTimeUsed 	= ntohl(status.nTotalFecthedTimeUsed);
	status.nTotalFinishedTasks 		= ntohl(status.nTotalFinishedTasks);
	status.nTotalFailedTasks 		= ntohl(status.nTotalFailedTasks);
	status.nTotalExecTimeUsed 		= ntohl(status.nTotalExecTimeUsed);
	status.nTotalFinishedResults 	= ntohl(status.nTotalFinishedResults);
	status.nTotalFailedResults 		= ntohl(status.nTotalFailedResults);
	status.nTotalWaitingResults 	= ntohl(status.nTotalWaitingResults);
	status.nTotalUploadedTimeUsed 	= ntohl(status.nTotalUploadedTimeUsed);
	status.nCurTaskID 				= ntohl(status.nCurTaskID);
	status.nLastFetchedTime 		= ntohl(status.nLastFetchedTime);
}

inline void Host2Net(_stClientStatus& status)
{
	status.nStartupTime				= htonl(status.nStartupTime);
	status.nTotalFetchedTasks 		= htonl(status.nTotalFetchedTasks);
	status.nTotalFecthedTimeUsed 	= htonl(status.nTotalFecthedTimeUsed);
	status.nTotalFinishedTasks 		= htonl(status.nTotalFinishedTasks);
	status.nTotalFailedTasks 		= htonl(status.nTotalFailedTasks);
	status.nTotalExecTimeUsed 		= htonl(status.nTotalExecTimeUsed);
	status.nTotalFinishedResults 	= htonl(status.nTotalFinishedResults);
	status.nTotalFailedResults 		= htonl(status.nTotalFailedResults);
	status.nTotalWaitingResults 	= htonl(status.nTotalWaitingResults);
	status.nTotalUploadedTimeUsed 	= htonl(status.nTotalUploadedTimeUsed);
	status.nCurTaskID 				= htonl(status.nCurTaskID);
	status.nLastFetchedTime 		= htonl(status.nLastFetchedTime);
}


///////////////////////////////////////////////////////


//App版本更新信息
class CAppUpdateInfo
{
public:
	string				sAppType;			//App应用程序类型
	int					nAppUpdateVer;		//待升级的版本号
	string				sAppUpdateFile;		//待升级的升级包压缩文件（含路径）

public:
	CAppUpdateInfo();
};


//master端配置信息
class CMasterConf
{
public:
	CMasterConf(const char* sParamFile, const char* sInstallPath);
	virtual ~CMasterConf();

public:
	string	sIP;					//server监听IP
	int		nPort;					//server监听端口

	int		nHttpdEnabled;			//是否开启HTTP服务
	int		nHttpdPort;				//HTTP服务端口

	string	sClientUpdateDir;		//Client节点应用程序升级包存放路径
	string	sResultUpdateDir;		//Result节点应用程序升级包存放路径
	int		nVerWatchInterval;		//每隔多少秒扫描是否有应用程序升级包
	
	int		nAsynMsgProcessing;		//非0时进行异步消息处理
	int		nMaxListenerCount;		//监听服务组中的监听服务最大数，缺省为3
	string	sTaskPath;				//任务存放路径，缺省为“./task/”
	string	sZipTaskPath;			//任务压缩保存路径，缺省为”./task/"
	int		nTaskCreateStrategy;	//任务生成策略
	int		nMaxTaskFetchTime;		//任务下发最长时间，单位为秒，缺省为300秒
	int		nMaxTaskRunTime;		//任务执行最长时间，单位为妙，缺省为3600秒
	int		nMaxSaveStateTime;		//多长时间保存一次Client节点状态，缺省为600秒
	int		nMaxPacketSize;			//任务下载时传输的数据包的最大长度（字节数）
	int		nAppRunTimeout;			//任务生成程序执行时限（秒）
	int		nAutoDeleteTaskFile;	//非0时自动删除已处理完的任务压缩文件
	int		nTaskFailStrategy;		//失败任务的处理策略
	int		nTaskFailMaxRetry;		//失败任务的重试次数
	int		nAutoSaveUnfinishedTask;//非0时在退出时记录未处理和失败的任务信息
	int		nSourceStatusInterval;	//每隔多长时间监控资源使用情况

public:
	int		nTaskPort;				//任务下载服务端口（系统自动获得）
	string	sInstallPath;			//部署路径（外部传入）
	time_t	nStartupTime;			//启动时间（系统自动获得）

public:
	map<string,string> oAppType2AppCmd;	//保存每个AppType的AppCmd

public:
	//Client节点App版本管理信息
	map<string, CAppUpdateInfo*> oClientAppVersion; //first为AppType, second为对应的升级版本信息
	DEFINE_LOCKER(locker4ClientApp);

	//Result节点App版本管理信息
	map<string, CAppUpdateInfo*> oResultAppVersion; //first为AppType, second为对应的升级版本信息
	DEFINE_LOCKER(locker4ResultApp);
};


//Result节点配置信息
class CResultConf
{
public:
	CResultConf(const char* sConfFile, const char* sInstallPath);

public:
	int		nResultID;				//Result节点ID
	string	sListenIP;				//监听IP
	int		nListenPort;			//监听端口
	int		nNatEnabled;			//是否为NAT网络环境
	string	sNatIP;					//NAT转换后的IP
	string	sAppType;				//应用程序类型
	int		nUploadErrorLog;		//非0时异常日志将回传给Master节点
	int		nUpdateEnabled;			//非0时开启版本自动更新功能
	string	sUpdateTargetPath;		//升级目标路径（即升级包中的文件存放在何处）
	int		nUpdateInterval;		//每隔多少秒检查是否需要升级
	string	sZipUpdateDir;			//升级包压缩文件的临时存放路径
	int		nAppRunTimeout;			//结果处理程序处理时限
	int		nResultFailMaxRetry;	//结果处理失败时最大重试次数
	string	sZipResultDir;			//结果保存文件夹
	string	sDataDir;				//将上传的结果解压这个目录提供给入库程序扫描
	int		nHeartbeatInterval;		//心跳间隔周期
	int		nMaxPacketSize;			//结果上传时传输的数据包的最大长度（字节数）
	int		nUploadTimeout;			//结果上传最大耗时
	int		nAutoDeleteResultFile;	//非0时,自动删除已上传完的结果文件
	int		nAutoSaveUnfinishedResultFile; //非0时自动保存未处理的结果文件以便下次启动时重新载入
	int		nSourceStatusInterval;	//每隔多长时间监控资源使用情况

	string	sInstallPath;			//部署路径（系统自动获得）
	time_t	nStartupTime;			//启动时间（系统自动获得）

public: //服务端IP和端口
	string	sMasterIP;
	int		nMasterPort;
	string	sBakMasterIP;
	int		nBakMasterPort;

public:
	map<string,string> oAppType2AppCmd;	//保存每个AppType的AppCmd（内部管理用）
	map<string, int> oCurAppVersion; //first为AppType, second为当前版本号
};


//Client节点配置信息
class CClientConf
{
public:
	CClientConf(const char* sConfFile, const char* sProListFile, const char* sInstallPath);

public:
	int		nClientID;				//节点ID
	string	sAppCmd;				//xxx.exe INPUT OUTPUT
	string	sAppType;				//节点应用程序类型
	int		nResultUploadEnabled;	//非0时上传任务执行结果
	int		nUploadErrorLog;		//非0时异常日志将回传给Master节点
	int		nUpdateEnabled;			//非0时开启版本自动更新功能
	string	sUpdateTargetPath;		//升级目标路径（即升级包中的文件存放在何处）
	int		nUpdateInterval;		//每隔多少秒检查是否需要升级
	string	sZipUpdateDir;			//升级包压缩文件的临时存放路径
	string	sInputDir;				//输入文件夹
	string	sOutputDir;				//输出文件夹
	string	sZipTaskDir;			//任务压缩文件夹
	string	sZipResultDir;			//结果压缩文件夹
	int		nHeartbeatInterval;		//心跳间隔周期
	int		nStateInterval;			//状态发送时间间隔
	int		nTaskReqWaitTime;		//任务执行完后隔多长时间再请求新任务
	int		nAppRunTimeout;			//最大杀死进程时间
	int		nAsynUpload;			//为1时表示异步上传，为0表示同步上传
	int		nMaxWaitingUploadFiles;	//异步上传时，等待上传的文件个数最大值
	int		nAutoDeleteResultFile;	//非0时,自动删除已上传完的结果文件
	int		nAutoDeleteTaskFile;	//非0时自动删除已处理完的任务文件
	int		nRememberResultAddr;	//非0时记录首次上传结果时的Result节点地址（此后都将使用该地址除非该Result节点断开）
	int		nTaskReqTimeInterval;	//请求任务失败时隔多长时间再请求（单位为秒）
	int		nAutoSaveUploadFile;	//非0时在Client节点终止时自动保存未上传的结果文件以便下次启动时重新载入
	int		nSourceStatusInterval;	//每隔多长时间监控资源使用情况
	
	
	string	sAppCmdFile;			//命令对应的文件名（系统自动获得）
	string	sInstallPath;			//部署路径（系统自动获得）
	time_t	nStartupTime;			//启动时间（系统自动获得）
	string	sAppVerFile;			//应用程序版本号文件（内部管理）
	int		nCurAppVersion;			//当前应用程序版本号（内部管理）

public:
	string	sMasterIP;				//Master端的IP地址
	int		nMasterPort;			//Master端的端口
	string	sBakMasterIP;			//备用Master端的IP地址
	int		nBakMasterPort;			//备用Master端的端口

public:
	
	class CProInfo
	{
	public:
		string sProPath;			//进程的执行路径
		string sProName;			//进程名
	};

	vector<CProInfo> oAppProcessList; //应用程序运行时的进程列表（用于节点程序运行异常时杀死）

private:
	string m_sConfFile;				//Client节点配置文件(client.ini)
	string m_sProListFile;			//节点进程配置文件(killprolist.ini)
};

//任务统计信息
class CTaskStatInfo
{
public:
	unsigned int		nTotalCreatedTasks;		//已创建任务数
	unsigned int		nTotalCreatedTimeUsed;	//创建任务总耗时
	unsigned int		nLastCreatedTaskID;		//最近一次创建的任务ID
	time_t				nLastCreatedTime;		//最近一次创建任务时间
	string				sLastCreatedTaskFile;	//最近一次创建的任务文件
	unsigned int		nTotalDeliveredTasks;	//已分发任务数
	unsigned int		nTotalFinishdTasks;		//已完成任务数
	unsigned int		nTotalFailedTasks;		//已失败任务数
	
public:
	DEFINE_LOCKER(m_locker);
	CTaskStatInfo();
	virtual ~CTaskStatInfo();
};

//Master节点用于进行Result节点管理的信息
class CResultNodeInfo
{
public:
	int					nResultID;			//节点ID
	_stResultNode*		pResultNode;		//基本信息
	string				sInstallPath;		//Result节点部署路径（绝对路径）
	time_t				nRegisterTime;		//注册到Master节点的时间
	time_t				nLastActiveTime;	//最近活跃时间
	_stResultStatus		dNodeStatus;		//节点运行状态信息
	_stNodeSourceStatus dSourceStatus;		//资源使用状况信息
	list<string>		dErrLogs;			//异常日志

	SOCKET				nHeartSocket;		//心跳连接
	bool				bDisabled;			//是否无效
	time_t				nDisabledTime;		//无效起始时间

public:
	CResultNodeInfo();
	virtual ~CResultNodeInfo();
};

//Client节点信息
class CClientInfo
{
public:
	int					nClientID;			//节点ID
	string				sAppType;			//节点应用类型(如"NewsGather")
	string				sInstallPath;		//Client节点部署路径（绝对路径）
	string				sIP;				//Client节点IP
	unsigned short		nPort;				//Client节点端口号（心跳连接端口）

	_stClientState		dAppState;			//应用程序状态信息
	time_t				tLastUpdateTime;	//Client节点上次状态更新的时间

public:
	time_t				nRegisterTime;		//注册时间
	time_t				nLastActiveTime;	//最近活跃时间
	bool				bTaskRequested;		//是否请求了任务
	int					nCurTaskID;			//当前正在执行的任务ID
	_stClientStatus		dNodeStatus;		//节点运行状态信息
	_stNodeSourceStatus dSourceStatus;		//资源使用状况信息
	list<string>		dErrLogs;			//异常日志

	SOCKET				nHeartSocket;		//心跳连接
	bool				bDisabled;			//是否无效
	time_t				nDisabledTime;		//无效起始时间

public:
	CClientInfo();
};


//解析出AppType和InstallPath（分隔符为:）
void ParseAppTypeAndInstallPath(const char* s, string& sAppType, string& sInstallPath);

//获得资源使用情况：CPU/Memory/DISK/NIC
bool GetSourceStatusInfo(_stNodeSourceStatus& status);

//从版本号文件中读取和写入版本号
int ReadAppVersion(const string& sVerFile, const string& sAppType);
bool WriteAppVersion(const string& sVerFile, const string& sAppType, int nVersion);

#endif //_H_TYPEDEFS_GDDING_INCLUDED_20100204

