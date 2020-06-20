#include <stdio.h>   
#include <event.h>
#include <evhttp.h>
#include "HttpServer.h"
#include "MasterNode.h"
#include "TypeDefs.h"
#include "../utils/RunLogger.h"
#include "../utils/Utility.h"


static struct event_base *g_pEventBase = NULL;
static struct evhttp *g_pHttp = NULL;

#define OUTPUT_HTTP_HEADER(content_type) \
	evhttp_add_header(req->output_headers, "Server", "MARC-HTTP-SERVER-0.0.1");\
	evhttp_add_header(req->output_headers, "Content-Type", content_type);\
	evhttp_add_header(req->output_headers, "Connection", "close")

#define OUTPUT_HTTP_DATA(req,html) \
	struct evbuffer *buf = evbuffer_new();\
	if(buf != NULL)\
	{\
		evbuffer_add_printf(buf, (html));\
		evhttp_send_reply((req), HTTP_OK, "OK", buf);\
		evbuffer_free(buf);\
	}\
	else\
	{\
		evhttp_send_error(req, 501, "Server error: evbuffer_new failed");\
	}\


static const struct table_entry
{
	const char *extension;
	const char *content_type;
} content_type_table[] = 
{
	{ "txt", "text/plain" },
	{ "c", "text/plain" },
	{ "h", "text/plain" },
	{ "html", "text/html" },
	{ "htm", "text/html" },
	{ "css", "text/css" },
	{ "gif", "image/gif" },
	{ "jpg", "image/jpeg" },
	{ "jpeg", "image/jpeg" },
	{ "png", "image/png" },
	{ "pdf", "application/pdf" },
	{ "ps", "application/postsript" },
	{ NULL, NULL },
};

/* Try to guess a good content-type for 'path' */
static const char* guess_content_type(const char *path)
{
	const char *last_period, *extension;
	const struct table_entry *ent;
	last_period = strrchr(path, '.');
	if (!last_period || strchr(last_period, '/'))
		goto not_found; /* no exension */
	extension = last_period + 1;
	for (ent = &content_type_table[0]; ent->extension; ++ent) 
	{
		if (!evutil_ascii_strcasecmp(ent->extension, extension))
			return ent->content_type;
	}

not_found:
	return "application/misc";
}


void CHttpServer::show_file(struct evhttp_request *req, void *arg)
{
	CHttpServer* me = (CHttpServer*)arg;
        assert(me != NULL);

	//get content-type
	string sFile("./http");
	sFile += req->uri;
	if(strcmp(req->uri, "/") == 0)
		sFile += "index.html";
	const char *content_type = guess_content_type(sFile.c_str());

	//check last modified
	string sModifiedSince = "";
	struct evkeyvalq *headers = evhttp_request_get_input_headers(req);
	struct evkeyval *header;
	for (header = headers->tqh_first; header; header = header->next.tqe_next) 
	{
		if (stricmp(header->key, "If-Modified-Since") == 0 &&
			stricmp(header->value, me->m_sLastModified.c_str()) == 0)
		{
			OUTPUT_HTTP_HEADER(content_type);
			evhttp_send_reply(req, HTTP_NOTMODIFIED, "not modified", NULL);
			return ;
		}
	}

	//HTTP header
	OUTPUT_HTTP_HEADER(content_type);
	evhttp_add_header(req->output_headers, "Last-Modified", me->m_sLastModified.c_str());

	char* pFileBuf = 0;
	int nFileSize = readFile(sFile.c_str(), pFileBuf);
	if(nFileSize >= 0)
	{
		struct evbuffer *buf = evbuffer_new();
		if(buf != NULL)
		{
			evbuffer_add(buf, pFileBuf, nFileSize);
			evhttp_send_reply(req, HTTP_OK, "OK", buf);
			evbuffer_free(buf);
		}
		else
		{
			evhttp_send_error(req, 501, "Server error: evbuffer_new failed");
		}
		free(pFileBuf);
	}
	else
	{
		evhttp_send_error(req, 404, "Document was not found");
	}
}

void CHttpServer::show_master_info(struct evhttp_request *req, void *arg)
{
	CHttpServer* me = (CHttpServer*)arg;
	assert(me != NULL);

	//HTTP header
	OUTPUT_HTTP_HEADER("text/html; charset=gb2312");

	//输出的内容
	string html = "";
	me->m_pMasterNode->Dump2Html(DUMP_MASTER_NODE_INFO, html);
	OUTPUT_HTTP_DATA(req, html.c_str());
}

void CHttpServer::show_result_info(struct evhttp_request *req, void *arg)
{
	CHttpServer* me = (CHttpServer*)arg;
	assert(me != NULL);

	//HTTP header
	OUTPUT_HTTP_HEADER("text/html; charset=gb2312");

	//输出的内容
	string html = "";
	me->m_pMasterNode->Dump2Html(DUMP_RESULT_NODE_INFO, html);
	OUTPUT_HTTP_DATA(req, html.c_str());
}

void CHttpServer::show_client_info(struct evhttp_request *req, void *arg)
{
	CHttpServer* me = (CHttpServer*)arg;
	assert(me != NULL);

	//HTTP header
	OUTPUT_HTTP_HEADER("text/html; charset=gb2312");

	//输出的内容
	string html = "";
	me->m_pMasterNode->Dump2Html(DUMP_CLIENT_NODE_INFO, html);
	OUTPUT_HTTP_DATA(req, html.c_str());
}

void CHttpServer::show_task_info(struct evhttp_request *req, void *arg)
{
	CHttpServer* me = (CHttpServer*)arg;
	assert(me != NULL);

	//HTTP header
	OUTPUT_HTTP_HEADER("text/html; charset=gb2312");

	//输出的内容
	string html = "";
	me->m_pMasterNode->Dump2Html(DUMP_TASK_INFO, html);
	OUTPUT_HTTP_DATA(req, html.c_str());
}

void CHttpServer::show_client_log(struct evhttp_request *req, void *arg)
{
	CHttpServer* me = (CHttpServer*)arg;
	assert(me != NULL);

	//HTTP header
	OUTPUT_HTTP_HEADER("text/html; charset=gb2312");

	//得到ClientID
	const char *uri = evhttp_request_uri(req);
	struct evkeyvalq params;
	evhttp_parse_query(uri, &params);
	const char* sClientID = evhttp_find_header(&params, "id");
	int nClientID = (sClientID==NULL ? 0 : atoi(sClientID));

	//输出的内容
	string html = "";
	me->m_pMasterNode->Dump2Html(DUMP_CLIENT_LOG_INFO, html, nClientID);
	OUTPUT_HTTP_DATA(req, html.c_str());
}

void CHttpServer::show_client_errlog(struct evhttp_request *req, void *arg)
{
	CHttpServer* me = (CHttpServer*)arg;
	assert(me != NULL);

	//HTTP header
	OUTPUT_HTTP_HEADER("text/html; charset=gb2312");

	//得到ClientID
	const char *uri = evhttp_request_uri(req);
	struct evkeyvalq params;
	evhttp_parse_query(uri, &params);
	const char* sClientID = evhttp_find_header(&params, "id");
	int nClientID = (sClientID==NULL ? 0 : atoi(sClientID));

	//输出的内容
	string html = "";
	me->m_pMasterNode->Dump2Html(DUMP_CLIENT_ERRLOG_INFO, html, nClientID);
	OUTPUT_HTTP_DATA(req, html.c_str());
}

void CHttpServer::show_result_log(struct evhttp_request *req, void *arg)
{
	CHttpServer* me = (CHttpServer*)arg;
	assert(me != NULL);

	//HTTP header
	OUTPUT_HTTP_HEADER("text/html; charset=gb2312");

	//得到ResultID
	const char *uri = evhttp_request_uri(req);
	struct evkeyvalq params;
	evhttp_parse_query(uri, &params);
	const char* sResultID = evhttp_find_header(&params, "id");
	int nResultID = (sResultID==NULL ? 0 : atoi(sResultID));

	//输出的内容
	string html = "";
	me->m_pMasterNode->Dump2Html(DUMP_RESULT_LOG_INFO, html, nResultID);
	OUTPUT_HTTP_DATA(req, html.c_str());
}

void CHttpServer::show_result_errlog(struct evhttp_request *req, void *arg)
{
	CHttpServer* me = (CHttpServer*)arg;
	assert(me != NULL);

	//HTTP header
	OUTPUT_HTTP_HEADER("text/html; charset=gb2312");

	//得到ResultID
	const char *uri = evhttp_request_uri(req);
	struct evkeyvalq params;
	evhttp_parse_query(uri, &params);
	const char* sResultID = evhttp_find_header(&params, "id");
	int nResultID = (sResultID==NULL ? 0 : atoi(sResultID));

	//输出的内容
	string html = "";
	me->m_pMasterNode->Dump2Html(DUMP_RESULT_ERRLOG_INFO, html, nResultID);
	OUTPUT_HTTP_DATA(req, html.c_str());
}

void CHttpServer::show_master_log(struct evhttp_request *req, void *arg)
{
	CHttpServer* me = (CHttpServer*)arg;
	assert(me != NULL);

	//HTTP header
	OUTPUT_HTTP_HEADER("text/html; charset=gb2312");

	//输出的内容
	string html = "";
	me->m_pMasterNode->Dump2Html(DUMP_MASTER_LOG_INFO, html);
	OUTPUT_HTTP_DATA(req, html.c_str());
}

void CHttpServer::show_master_errlog(struct evhttp_request *req, void *arg)
{
	CHttpServer* me = (CHttpServer*)arg;
	assert(me != NULL);

	//HTTP header
	OUTPUT_HTTP_HEADER("text/html; charset=gb2312");

	//输出的内容
	string html = "";
	me->m_pMasterNode->Dump2Html(DUMP_MASTER_ERRLOG_INFO, html);
	OUTPUT_HTTP_DATA(req, html.c_str());
}



CHttpServer::CHttpServer(CMasterNode* pMasterNode, CRunLogger* pLogger)
{
	m_pMasterNode = pMasterNode;
	m_pLogger = pLogger;

	char date[50];
#ifndef WIN32
	struct tm cur;
#endif
	struct tm *cur_p;
	time_t t = time(0);
#ifdef WIN32
	cur_p = gmtime(&t);
#else
	gmtime_r(&t, &cur);
	cur_p = &cur;
#endif
	if (strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", cur_p) != 0) 
	{
		m_sLastModified = date;
	}
}

CHttpServer::~CHttpServer()
{
}

bool CHttpServer::Start()
{
	struct evhttp_bound_socket *handle = NULL;
	if(g_pEventBase == NULL)
	{
		g_pEventBase = event_base_new();
		if(g_pEventBase == NULL)
		{
			m_pLogger->Write(CRunLogger::LOG_ERROR, "event_base_new() failed!\n");
			goto _ERROR;
		}
	}

	if(g_pHttp == NULL)
	{
		g_pHttp = evhttp_new(g_pEventBase);
		if(g_pHttp == NULL) 
		{
			m_pLogger->Write(CRunLogger::LOG_ERROR, "evhttp_new() failed!\n");
			goto _ERROR;
		}
	}

	evhttp_set_cb(g_pHttp, "/master", show_master_info, this);
	evhttp_set_cb(g_pHttp, "/result", show_result_info, this);
	evhttp_set_cb(g_pHttp, "/client", show_client_info, this);
	evhttp_set_cb(g_pHttp, "/task", show_task_info, this);
	evhttp_set_cb(g_pHttp, "/client_log", show_client_log, this);
	evhttp_set_cb(g_pHttp, "/client_errlog", show_client_errlog, this);
	evhttp_set_cb(g_pHttp, "/result_log", show_result_log, this);
	evhttp_set_cb(g_pHttp, "/result_errlog", show_result_errlog, this);
	evhttp_set_cb(g_pHttp, "/master_log", show_master_log, this);
	evhttp_set_cb(g_pHttp, "/master_errlog", show_master_errlog, this);
	evhttp_set_gencb(g_pHttp, show_file, this);

	/* Now we tell the evhttp what port to listen on */
	handle = evhttp_bind_socket_with_handle(g_pHttp, m_pMasterNode->HttpdIP().c_str(), m_pMasterNode->HttpdPort());
	if(handle == NULL)
	{
		m_pLogger->Write(CRunLogger::LOG_ERROR, "evhttp_bind_socket_with_handle() failed!\n");
		goto _ERROR;
	}

	event_base_dispatch(g_pEventBase);
	return true;

_ERROR:
	if(g_pHttp != NULL)
	{
		evhttp_free(g_pHttp);
		g_pHttp = NULL;
	}
	if(g_pEventBase != NULL)
	{
		event_base_free(g_pEventBase);
		g_pEventBase = NULL;
	}
	return false;

	return true;
}

void CHttpServer::Stop()
{
	if(g_pHttp != NULL)
	{
		evhttp_free(g_pHttp);
		g_pHttp = NULL;
	}
	if(g_pEventBase != NULL)
	{
		event_base_loopbreak(g_pEventBase);
		event_base_free(g_pEventBase);
		g_pEventBase = NULL;
	}
}
