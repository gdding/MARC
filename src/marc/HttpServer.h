/*-----------------------------------------------------------------------------
* Copyright (c) 2010~2011 ICT, CAS. All rights reserved.
*   dingguodong@ict.ac.cn OR gdding@hotmail.com
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.
*-----------------------------------------------------------------------------*/
#ifndef _H_HTTPSERVER_GDDING_INCLUDED_20110708
#define _H_HTTPSERVER_GDDING_INCLUDED_20110708
#include <string>
using ::std::string;

class CMasterNode;
class CRunLogger;

class CHttpServer
{
public:
	CHttpServer(CMasterNode* pMasterNode, CRunLogger* pLogger);
	virtual ~CHttpServer();

public:
	bool Start();
	void Stop();

private:
	static void show_file(struct evhttp_request *req, void *arg);
	static void show_master_info(struct evhttp_request *req, void *arg);
	static void show_result_info(struct evhttp_request *req, void *arg);
	static void show_client_info(struct evhttp_request *req, void *arg);
	static void show_task_info(struct evhttp_request *req, void *arg);

	static void show_master_log(struct evhttp_request *req, void *arg);
	static void show_master_errlog(struct evhttp_request *req, void *arg);
	static void show_result_log(struct evhttp_request *req, void *arg);
	static void show_result_errlog(struct evhttp_request *req, void *arg);
	static void show_client_log(struct evhttp_request *req, void *arg);
	static void show_client_errlog(struct evhttp_request *req, void *arg);
	
private:
	CMasterNode*			m_pMasterNode;
	CRunLogger*				m_pLogger;
	string					m_sLastModified;
};


#endif //_H_HTTPSERVER_GDDING_INCLUDED_20110708
