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


//ϵͳ����Ҫ�Ĳ���
#define MARC_MAX_MSG_BYTES							512				//512Byte
#define MARC_MAX_STATE_SIZE							500				//�ڵ�״̬��Ϣ��󳤶�
#define MARC_MAX_CONN_COUNT							256				//���������
#define MARC_HEART_TIMEOUT							300				//������ⳬʱ���룩
#define MARC_SHORT_CONN_TIMEOUT						300				//�������������ڣ��룩
#define MARC_TASKCREATE_TIME_INTERVAL				60				//���񴴽�ʧ��ʱ���೤ʱ���ٴ������룩
#define MARC_TASKWATCH_TIME_INTERVAL				5				//����ִ��������ʱ�������룩
#define MARC_TASKFINISHED_KEEP_TIME					3600			//����ɵ���������Ϣ���ڴ��б���ʱ�䣨�룩
#define MARC_TASKFINISHED_KEEP_SIZE					20				//����ɵ���������Ϣ���ڴ��б����ĸ���
#define MARC_TASKIGNORED_KEEP_TIME					(24*3600)		//����������������Ϣ���ڴ��б���ʱ�䣨�룩
#define MARC_MAX_KEEP_LOGINFO						100				//�ڴ�����ౣ������־��Ŀ
#define MARC_MAGIC_NUMBER							0x0123			//ϵͳ����ħ��

//��ʱ�ļ�
#define MARC_REUPLOAD_RESULT_LISTFILE				"marc_reupload.result"		//�ش�����ļ��б�
#define MARC_UNFINISHED_TASK_LISTFILE				"marc_unfinished.task"		//δ��������ļ��б�
#define MARC_UNFINISHED_RESULT_LISTFILE				"marc_unfinished.result"	//δ��ɽ���ļ��б�

//�����ļ�
#define MARC_MASTER_CONF_FILE						"master.ini"
#define MARC_RESULT_CONF_FILE						"result.ini"
#define MARC_CLIENT_CONF_FILE						"client.ini"
#define MARC_KILLPROLIST_CONF_FILE					"killprolist.ini"

//����ɱ���ű�
#ifdef _WIN32
	#define MARC_KILLPROLIST_SCRIPT_FILE			"killpro.bat"
#else
	#define MARC_KILLPROLIST_SCRIPT_FILE			"./killpro.sh"
#endif

//��ȡ��Դʹ��״���Ľű�(CPU/DISK/MEMORY/NIC)
#ifdef _WIN32
	#define MARC_CDMN_SCRIPT_FILE					"marc_cdmn.bat"
	#define MARC_CDMN_OUTPUT_FILE					"marc_cdmn.txt"
#else
	#define MARC_CDMN_SCRIPT_FILE					"./marc_cdmn.sh"
	#define MARC_CDMN_OUTPUT_FILE					"marc_cdmn.txt"
#endif

//�ļ�/Ŀ¼ѹ������
#ifdef WIN32
	#define MARC_MYZIP_APPCMD "myzip.exe"
#else
	#define MARC_MYZIP_APPCMD "./myzip"
#endif

/**
* CClientNode, CResultNode��CMasterNode֮���ͨѶ����
* C2M: CClientNode --> CMasterNode
* M2C: CMasterNode --> CClientNode
* R2M: CResultNode --> CMasterNode
* M2R: CMasterNode --> CResultNode
**/
#define C2M_CMD_REGISTER_REQ						4000        //Client�ڵ���Master�ڵ�����ע��
#define M2C_CMD_REGISTER_YES						4001        //Master�ڵ����Client�ڵ�ע��ɹ�
#define M2C_CMD_REGISTER_NO							4002        //Master�ڵ����Client�ڵ㲻����ע��
#define R2M_CMD_REGISTER_REQ						4010        //Result�ڵ���Master����ע��
#define M2R_CMD_REGISTER_YES						4011        //Master�ڵ����Result�ڵ�ע��ɹ�
#define M2R_CMD_REGISTER_NO							4012        //Master�ڵ����Result�ڵ㲻����ע��
#define R2M_CMD_UNREGISTER							4013		//Result�ڵ�ע��
#define R2M_CMD_INSTALL_PATH						4014		//Result�ڵ����Master�䲿��·��
#define R2M_CMD_HEART_SEND							4020        //Result�ڵ㷢����������
#define R2M_CMD_SOURCE_STATUS						4021		//Result�ڵ㷢����Դʹ��״��
#define R2M_CMD_ERRLOG_INFO							4022		//Result�ڵ㷢���쳣��־
#define R2M_CMD_APPVER_REQ							4030	    //Client�ڵ�����汾�˲�
#define M2R_CMD_APPVER_YES							4031        //Client�ڵ���ڸ��°汾
#define M2R_CMD_APPVER_NO							4032        //Client�ڵ㲻���ڸ��°汾
#define R2M_CMD_CLOSE								4040		//Result�ڵ�����ر�����
#define C2M_CMD_CLOSE								4041		//Client�ڵ�����ر�����

/**
* CClientNode��CMasterListener֮���ͨѶ����
* C2L: CClientNode -->CMasterListener
* L2C: CMasterListener->CClientNode
**/
#define C2L_CMD_REGISTER_REQ						5000		//Client�ڵ�����ע��
#define L2C_CMD_REGISTER_YES						5001        //����Client�ڵ�ע��ɹ�
#define L2C_CMD_REGISTER_NO							5002        //����Client�ڵ㲻��ע��
#define C2L_CMD_UNREGISTER							5003		//Client�ڵ�ע��
#define C2L_CMD_TASK_REQ							5010	    //Client�ڵ���������
#define L2C_CMD_TASK_YES							5011        //����Client�ڵ�������
#define L2C_CMD_TASK_NO								5012        //����Client�ڵ�û������
#define C2L_CMD_STATE_SEND							5013		//Client�ڵ㷢��״̬
#define C2L_CMD_HEART_SEND							5014		//Client�ڵ㷢��������Ϣ
#define C2L_CMD_APP_FINISHED						5015		//Client�ڵ����������������
#define C2L_CMD_APP_TIMEOUT							5016		//Client�ڵ�������г�ʱ
#define C2L_CMD_APP_FAILED							5017		//Client�ڵ��������ʧ��
#define C2L_CMD_TASKDOWN_FAILED						5018		//��������ʧ��
#define C2L_CMD_CLOSE								5019		//Client�ڵ�����ر�����
#define C2L_CMD_UPLOAD_REQ							5020		//Client�ڵ������ϴ��ļ�
#define L2C_CMD_UPLOAD_YES							5021        //Client�ڵ�����ϴ��ļ�
#define L2C_CMD_UPLOAD_NO							5022        //Client�ڵ㲻���ϴ��ļ�
#define C2L_CMD_TASKDOWN_SUCCESS					5023		//Client�ڵ��������سɹ�
#define C2L_CMD_SOURCE_STATUS						5024		//Client�ڵ㷢����Դʹ��״��
#define C2L_CMD_ERRLOG_INFO							5025		//Client�ڵ㷢���쳣��־
#define C2L_CMD_APPVER_REQ							5030	    //Client�ڵ�����汾�˲�
#define L2C_CMD_APPVER_YES							5031        //Client�ڵ���ڸ��°汾
#define L2C_CMD_APPVER_NO							5032        //Client�ڵ㲻���ڸ��°汾
#define L2C_CMD_INVALID_CLIENT						5555		//��֪client�ڵ�����Ч

//�����붨��
#define MARC_CODE_OK								0			//�ɹ�
#define MARC_CODE_INVALID_COMMAND					9002		//�Ƿ�������
#define MARC_CODE_SELECT_FAILED						9003		//select��������
#define MARC_CODE_SOCKET_EXCEPTION					9004		//socket�����쳣
#define MARC_CODE_CONNECT_FAILED					9101		//����Master�ڵ�ʧ��
#define MARC_CODE_SEND_FAILED						9102		//��Master�ڵ㷢������ʧ��
#define MARC_CODE_RECV_FAILED						9103		//��Master�ڵ��������ʧ��
#define MARC_CODE_REGISTER_REFUSED					9104		//Master�ڵ�ܾ�ע��
#define MARC_CODE_NOTASK							9105		//Master�ڵ�û������
#define MARC_CODE_NO_RESULT_SERVER					9106		//û�пɹ��ϴ���Result�ڵ�
#define MARC_CODE_DOWNLOAD_FILE_FAILED				9107		//�ļ�����ʧ��
#define MARC_CODE_UPLOAD_FILE_FAILED				9108		//�ļ��ϴ�ʧ��
#define MARC_CODE_INVALID_CLIENT					9109		//client��Ч

//�������ɻ���
#define MARC_TASKCREATE_WHEN_FREE					0			//ֻҪClient�޴��������������������
#define MARC_TASKCREATE_ONLYWHEN_REQUESTED			1			//���ҽ���Client�����������ҵ�ǰ�޴��������������������

//ʧ������Ĵ������nTaskFailStrategy
#define MARC_FAILEDTASK_STRATEGY_IGNORE				0			//����,���ٴ���
#define MARC_FAILEDTASK_STRATEGY_KEEP				1			//ֻ�ܱ�ͬһ��Client�ڵ�����ִ�и�����
#define MARC_FAILEDTASK_STRATEGY_AJUST				2			//����ͬһ���͵�����Client�ڵ�����ִ�и�����

typedef enum
{
	SHORT_CONN_DEFAULT		= 0,		//����������
	SHORT_CONN_TASKREQ		= 1,		//�����������ӣ������ӣ�
	SHORT_CONN_UPLOADREQ	= 2,		//����ش��������ӣ������ӣ�
	LONG_CONN_HEART			= 100,		//�������ӣ������ӣ�
}TConnType;

//��ʽ�������HTML
typedef enum
{
	DUMP_MASTER_NODE_INFO	= 1,		//���Master�ڵ���Ϣ
	DUMP_RESULT_NODE_INFO	= 2,		//���Result�ڵ���Ϣ
	DUMP_CLIENT_NODE_INFO	= 3,		//���Client�ڵ���Ϣ
	DUMP_TASK_INFO			= 4,		//�������ִ��״̬��Ϣ
	DUMP_MASTER_LOG_INFO	= 5,		//���Master��־��Ϣ
	DUMP_MASTER_ERRLOG_INFO	= 6,		//���Master�쳣��־��Ϣ
	DUMP_RESULT_LOG_INFO	= 7,		//���Master��־��Ϣ
	DUMP_RESULT_ERRLOG_INFO	= 8,		//���Master�쳣��־��Ϣ
	DUMP_CLIENT_LOG_INFO	= 9,		//���Master��־��Ϣ
	DUMP_CLIENT_ERRLOG_INFO	= 10,		//���Master�쳣��־��Ϣ
}TDumpInfoType;

//�ͻ���������Ϣ
typedef struct
{
	SOCKET			nSocket;			//ΪINVALID_SOCKETʱ��ʾ�������ѶϿ�
	TConnType		nConnType;			//��������
	string			sIP;				//�ͻ���IP��ַ
	unsigned short	nPort;				//�ͻ��˶˿�
	int				nClientID;			//�ͻ���ID
	int				nStartTime;			//���ӽ���ʱ��
	int				nLastActiveTime;	//���һ�λʱ��
}TClientConn;

/////////////////////////////////////////////////////
//��Ϣ���ṹ����
typedef struct
{
	int				nCommand;			//����
	int				nClientID;			//�ڵ�ID
	int				nOffset;			//ƫ������ĳЩʱ����������;��
	int				nBufSize;			//������
	char			cBuffer[MARC_MAX_MSG_BYTES+1];	//������
}_stDataPacket;

// �����ֽ���תΪ�����ֽ���
inline void Net2Host(_stDataPacket &packet)
{
	packet.nCommand 	= ntohl(packet.nCommand);
	packet.nClientID 	= ntohl(packet.nClientID);
	packet.nOffset 		= ntohl(packet.nOffset);
	packet.nBufSize 	= ntohl(packet.nBufSize);
}

// �����ֽ���תΪ�����ֽ���
inline void Host2Net(_stDataPacket &packet)
{
	packet.nCommand 	= htonl(packet.nCommand);
	packet.nClientID 	= htonl(packet.nClientID);
	packet.nOffset 		= htonl(packet.nOffset);
	packet.nBufSize 	= htonl(packet.nBufSize);
}
//////////////////////////////////////////////////////

///////////////////////////////////////////////////////
//Client�ڵ�״̬��Ϣ(�ܳ��Ȳ��ܳ���MARC_MAX_MSG_BYTES)
typedef struct
{
	int nBufSize;	 //״̬��Ϣ����
	char cBuffer[MARC_MAX_STATE_SIZE]; //״̬��Ϣ����
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
//��������������Ϣ(�ܳ��Ȳ��ܳ���MARC_MAX_MSG_BYTES)
typedef struct 
{
	unsigned short usPort;	//�������ط���˿�
	char chTaskFile[256];	//�����ļ���
	int nTaskID;			//����ID
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
//App�汾��Ϣ(�ܳ��Ȳ��ܳ���MARC_MAX_MSG_BYTES)
typedef struct
{
	unsigned short usPort;	//�汾���ط���˿�
	char chUpdateFile[256];	//�汾�������ļ���
	int nUpdateVersion;		//�����汾��
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
//Result�ڵ��ַ(�ܳ��Ȳ��ܳ���MARC_MAX_MSG_BYTES)
typedef struct 
{
	char chIp[16];			//IP��ַ
	unsigned short usPort;	//�˿�
	char chSavePath[256];	//�洢·��
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
//Result�ڵ�ע����Ϣ(�ܳ��Ȳ��ܳ���MARC_MAX_MSG_BYTES)
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

//�ڵ���Դʹ��״̬��Ϣ
typedef struct
{
	unsigned int	cpu_idle_ratio;		//CPU���аٷֱ�
	unsigned int	disk_avail_ratio;	//���ô��̰ٷֱ�
	unsigned int	memory_avail_ratio; //�����ڴ�ٷֱ�
	unsigned int	nic_bps;			//����������ÿ���ֽ�����
	unsigned int	watch_timestamp;	//��ȡʱ���
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

//Result�ڵ�״̬��Ϣ(�ܳ��Ȳ��ܳ���MARC_MAX_MSG_BYTES)
typedef struct
{
	unsigned int	nStartupTime;		//�ڵ�����ʱ��
	unsigned int	nOverload;			//���أ�������������
	unsigned int	nTotalReceived;		//�����������ܹ����յĽ����
	unsigned int	nTotalFinished;		//�����������ܹ��ɹ�����Ľ����
	unsigned int	nTotalTimeUsed;		//�����������ɹ��������ļ����ѵ�ʱ�䣨�룩
	unsigned int	nTotalFailed;		//�����������ܹ��������ʧ�ܵĴ���
	unsigned int	nTotalAbandoned;	//�����������ܹ�����������Ľ����
	unsigned int	nLastReceivedTime;	//���һ�ν��������ɵ�ʱ��
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

//Client�ڵ�����״̬��Ϣ���ܳ��Ȳ��ܳ���MARC_MAX_MSG_BYTES��
typedef struct
{
	unsigned int	nStartupTime;			//�ڵ�����ʱ��
	unsigned int	nTotalFetchedTasks;		//�ɹ����ص�������
	unsigned int	nTotalFecthedTimeUsed;	//�������ص��ܺ�ʱ���룩
	unsigned int	nTotalFinishedTasks;	//ִ�гɹ���������
	unsigned int	nTotalFailedTasks;		//ִ��ʧ�ܵ�������
	unsigned int	nTotalExecTimeUsed;		//����ִ���ܺ�ʱ���룩
	unsigned int	nTotalFinishedResults;	//�ϴ��ɹ��Ľ����
	unsigned int	nTotalFailedResults;	//�ϴ�ʧ�ܵĽ����
	unsigned int	nTotalWaitingResults;	//��ǰ���ϴ��Ľ����
	unsigned int	nTotalUploadedTimeUsed;	//����ϴ��ɹ����ܺ�ʱ���룩
	unsigned int	nCurTaskID;				//��ǰ��ִ�е�����ID����Ϊ0���ʾ�ڵ���У�
	unsigned int	nLastFetchedTime;		//���һ������������ɵ�ʱ��
	char			sLastFetchedFile[256];	//���һ�����سɹ��������ļ���
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


//App�汾������Ϣ
class CAppUpdateInfo
{
public:
	string				sAppType;			//AppӦ�ó�������
	int					nAppUpdateVer;		//�������İ汾��
	string				sAppUpdateFile;		//��������������ѹ���ļ�����·����

public:
	CAppUpdateInfo();
};


//master��������Ϣ
class CMasterConf
{
public:
	CMasterConf(const char* sParamFile, const char* sInstallPath);
	virtual ~CMasterConf();

public:
	string	sIP;					//server����IP
	int		nPort;					//server�����˿�

	int		nHttpdEnabled;			//�Ƿ���HTTP����
	int		nHttpdPort;				//HTTP����˿�

	string	sClientUpdateDir;		//Client�ڵ�Ӧ�ó������������·��
	string	sResultUpdateDir;		//Result�ڵ�Ӧ�ó������������·��
	int		nVerWatchInterval;		//ÿ��������ɨ���Ƿ���Ӧ�ó���������
	
	int		nAsynMsgProcessing;		//��0ʱ�����첽��Ϣ����
	int		nMaxListenerCount;		//�����������еļ��������������ȱʡΪ3
	string	sTaskPath;				//������·����ȱʡΪ��./task/��
	string	sZipTaskPath;			//����ѹ������·����ȱʡΪ��./task/"
	int		nTaskCreateStrategy;	//�������ɲ���
	int		nMaxTaskFetchTime;		//�����·��ʱ�䣬��λΪ�룬ȱʡΪ300��
	int		nMaxTaskRunTime;		//����ִ���ʱ�䣬��λΪ�ȱʡΪ3600��
	int		nMaxSaveStateTime;		//�೤ʱ�䱣��һ��Client�ڵ�״̬��ȱʡΪ600��
	int		nMaxPacketSize;			//��������ʱ��������ݰ�����󳤶ȣ��ֽ�����
	int		nAppRunTimeout;			//�������ɳ���ִ��ʱ�ޣ��룩
	int		nAutoDeleteTaskFile;	//��0ʱ�Զ�ɾ���Ѵ����������ѹ���ļ�
	int		nTaskFailStrategy;		//ʧ������Ĵ������
	int		nTaskFailMaxRetry;		//ʧ����������Դ���
	int		nAutoSaveUnfinishedTask;//��0ʱ���˳�ʱ��¼δ�����ʧ�ܵ�������Ϣ
	int		nSourceStatusInterval;	//ÿ���೤ʱ������Դʹ�����

public:
	int		nTaskPort;				//�������ط���˿ڣ�ϵͳ�Զ���ã�
	string	sInstallPath;			//����·�����ⲿ���룩
	time_t	nStartupTime;			//����ʱ�䣨ϵͳ�Զ���ã�

public:
	map<string,string> oAppType2AppCmd;	//����ÿ��AppType��AppCmd

public:
	//Client�ڵ�App�汾������Ϣ
	map<string, CAppUpdateInfo*> oClientAppVersion; //firstΪAppType, secondΪ��Ӧ�������汾��Ϣ
	DEFINE_LOCKER(locker4ClientApp);

	//Result�ڵ�App�汾������Ϣ
	map<string, CAppUpdateInfo*> oResultAppVersion; //firstΪAppType, secondΪ��Ӧ�������汾��Ϣ
	DEFINE_LOCKER(locker4ResultApp);
};


//Result�ڵ�������Ϣ
class CResultConf
{
public:
	CResultConf(const char* sConfFile, const char* sInstallPath);

public:
	int		nResultID;				//Result�ڵ�ID
	string	sListenIP;				//����IP
	int		nListenPort;			//�����˿�
	int		nNatEnabled;			//�Ƿ�ΪNAT���绷��
	string	sNatIP;					//NATת�����IP
	string	sAppType;				//Ӧ�ó�������
	int		nUploadErrorLog;		//��0ʱ�쳣��־���ش���Master�ڵ�
	int		nUpdateEnabled;			//��0ʱ�����汾�Զ����¹���
	string	sUpdateTargetPath;		//����Ŀ��·�������������е��ļ�����ںδ���
	int		nUpdateInterval;		//ÿ�����������Ƿ���Ҫ����
	string	sZipUpdateDir;			//������ѹ���ļ�����ʱ���·��
	int		nAppRunTimeout;			//������������ʱ��
	int		nResultFailMaxRetry;	//�������ʧ��ʱ������Դ���
	string	sZipResultDir;			//��������ļ���
	string	sDataDir;				//���ϴ��Ľ����ѹ���Ŀ¼�ṩ��������ɨ��
	int		nHeartbeatInterval;		//�����������
	int		nMaxPacketSize;			//����ϴ�ʱ��������ݰ�����󳤶ȣ��ֽ�����
	int		nUploadTimeout;			//����ϴ�����ʱ
	int		nAutoDeleteResultFile;	//��0ʱ,�Զ�ɾ�����ϴ���Ľ���ļ�
	int		nAutoSaveUnfinishedResultFile; //��0ʱ�Զ�����δ����Ľ���ļ��Ա��´�����ʱ��������
	int		nSourceStatusInterval;	//ÿ���೤ʱ������Դʹ�����

	string	sInstallPath;			//����·����ϵͳ�Զ���ã�
	time_t	nStartupTime;			//����ʱ�䣨ϵͳ�Զ���ã�

public: //�����IP�Ͷ˿�
	string	sMasterIP;
	int		nMasterPort;
	string	sBakMasterIP;
	int		nBakMasterPort;

public:
	map<string,string> oAppType2AppCmd;	//����ÿ��AppType��AppCmd���ڲ������ã�
	map<string, int> oCurAppVersion; //firstΪAppType, secondΪ��ǰ�汾��
};


//Client�ڵ�������Ϣ
class CClientConf
{
public:
	CClientConf(const char* sConfFile, const char* sProListFile, const char* sInstallPath);

public:
	int		nClientID;				//�ڵ�ID
	string	sAppCmd;				//xxx.exe INPUT OUTPUT
	string	sAppType;				//�ڵ�Ӧ�ó�������
	int		nResultUploadEnabled;	//��0ʱ�ϴ�����ִ�н��
	int		nUploadErrorLog;		//��0ʱ�쳣��־���ش���Master�ڵ�
	int		nUpdateEnabled;			//��0ʱ�����汾�Զ����¹���
	string	sUpdateTargetPath;		//����Ŀ��·�������������е��ļ�����ںδ���
	int		nUpdateInterval;		//ÿ�����������Ƿ���Ҫ����
	string	sZipUpdateDir;			//������ѹ���ļ�����ʱ���·��
	string	sInputDir;				//�����ļ���
	string	sOutputDir;				//����ļ���
	string	sZipTaskDir;			//����ѹ���ļ���
	string	sZipResultDir;			//���ѹ���ļ���
	int		nHeartbeatInterval;		//�����������
	int		nStateInterval;			//״̬����ʱ����
	int		nTaskReqWaitTime;		//����ִ�������೤ʱ��������������
	int		nAppRunTimeout;			//���ɱ������ʱ��
	int		nAsynUpload;			//Ϊ1ʱ��ʾ�첽�ϴ���Ϊ0��ʾͬ���ϴ�
	int		nMaxWaitingUploadFiles;	//�첽�ϴ�ʱ���ȴ��ϴ����ļ��������ֵ
	int		nAutoDeleteResultFile;	//��0ʱ,�Զ�ɾ�����ϴ���Ľ���ļ�
	int		nAutoDeleteTaskFile;	//��0ʱ�Զ�ɾ���Ѵ�����������ļ�
	int		nRememberResultAddr;	//��0ʱ��¼�״��ϴ����ʱ��Result�ڵ��ַ���˺󶼽�ʹ�øõ�ַ���Ǹ�Result�ڵ�Ͽ���
	int		nTaskReqTimeInterval;	//��������ʧ��ʱ���೤ʱ�������󣨵�λΪ�룩
	int		nAutoSaveUploadFile;	//��0ʱ��Client�ڵ���ֹʱ�Զ�����δ�ϴ��Ľ���ļ��Ա��´�����ʱ��������
	int		nSourceStatusInterval;	//ÿ���೤ʱ������Դʹ�����
	
	
	string	sAppCmdFile;			//�����Ӧ���ļ�����ϵͳ�Զ���ã�
	string	sInstallPath;			//����·����ϵͳ�Զ���ã�
	time_t	nStartupTime;			//����ʱ�䣨ϵͳ�Զ���ã�
	string	sAppVerFile;			//Ӧ�ó���汾���ļ����ڲ�����
	int		nCurAppVersion;			//��ǰӦ�ó���汾�ţ��ڲ�����

public:
	string	sMasterIP;				//Master�˵�IP��ַ
	int		nMasterPort;			//Master�˵Ķ˿�
	string	sBakMasterIP;			//����Master�˵�IP��ַ
	int		nBakMasterPort;			//����Master�˵Ķ˿�

public:
	
	class CProInfo
	{
	public:
		string sProPath;			//���̵�ִ��·��
		string sProName;			//������
	};

	vector<CProInfo> oAppProcessList; //Ӧ�ó�������ʱ�Ľ����б����ڽڵ���������쳣ʱɱ����

private:
	string m_sConfFile;				//Client�ڵ������ļ�(client.ini)
	string m_sProListFile;			//�ڵ���������ļ�(killprolist.ini)
};

//����ͳ����Ϣ
class CTaskStatInfo
{
public:
	unsigned int		nTotalCreatedTasks;		//�Ѵ���������
	unsigned int		nTotalCreatedTimeUsed;	//���������ܺ�ʱ
	unsigned int		nLastCreatedTaskID;		//���һ�δ���������ID
	time_t				nLastCreatedTime;		//���һ�δ�������ʱ��
	string				sLastCreatedTaskFile;	//���һ�δ����������ļ�
	unsigned int		nTotalDeliveredTasks;	//�ѷַ�������
	unsigned int		nTotalFinishdTasks;		//�����������
	unsigned int		nTotalFailedTasks;		//��ʧ��������
	
public:
	DEFINE_LOCKER(m_locker);
	CTaskStatInfo();
	virtual ~CTaskStatInfo();
};

//Master�ڵ����ڽ���Result�ڵ�������Ϣ
class CResultNodeInfo
{
public:
	int					nResultID;			//�ڵ�ID
	_stResultNode*		pResultNode;		//������Ϣ
	string				sInstallPath;		//Result�ڵ㲿��·��������·����
	time_t				nRegisterTime;		//ע�ᵽMaster�ڵ��ʱ��
	time_t				nLastActiveTime;	//�����Ծʱ��
	_stResultStatus		dNodeStatus;		//�ڵ�����״̬��Ϣ
	_stNodeSourceStatus dSourceStatus;		//��Դʹ��״����Ϣ
	list<string>		dErrLogs;			//�쳣��־

	SOCKET				nHeartSocket;		//��������
	bool				bDisabled;			//�Ƿ���Ч
	time_t				nDisabledTime;		//��Ч��ʼʱ��

public:
	CResultNodeInfo();
	virtual ~CResultNodeInfo();
};

//Client�ڵ���Ϣ
class CClientInfo
{
public:
	int					nClientID;			//�ڵ�ID
	string				sAppType;			//�ڵ�Ӧ������(��"NewsGather")
	string				sInstallPath;		//Client�ڵ㲿��·��������·����
	string				sIP;				//Client�ڵ�IP
	unsigned short		nPort;				//Client�ڵ�˿ںţ��������Ӷ˿ڣ�

	_stClientState		dAppState;			//Ӧ�ó���״̬��Ϣ
	time_t				tLastUpdateTime;	//Client�ڵ��ϴ�״̬���µ�ʱ��

public:
	time_t				nRegisterTime;		//ע��ʱ��
	time_t				nLastActiveTime;	//�����Ծʱ��
	bool				bTaskRequested;		//�Ƿ�����������
	int					nCurTaskID;			//��ǰ����ִ�е�����ID
	_stClientStatus		dNodeStatus;		//�ڵ�����״̬��Ϣ
	_stNodeSourceStatus dSourceStatus;		//��Դʹ��״����Ϣ
	list<string>		dErrLogs;			//�쳣��־

	SOCKET				nHeartSocket;		//��������
	bool				bDisabled;			//�Ƿ���Ч
	time_t				nDisabledTime;		//��Ч��ʼʱ��

public:
	CClientInfo();
};


//������AppType��InstallPath���ָ���Ϊ:��
void ParseAppTypeAndInstallPath(const char* s, string& sAppType, string& sInstallPath);

//�����Դʹ�������CPU/Memory/DISK/NIC
bool GetSourceStatusInfo(_stNodeSourceStatus& status);

//�Ӱ汾���ļ��ж�ȡ��д��汾��
int ReadAppVersion(const string& sVerFile, const string& sAppType);
bool WriteAppVersion(const string& sVerFile, const string& sAppType, int nVersion);

#endif //_H_TYPEDEFS_GDDING_INCLUDED_20100204

