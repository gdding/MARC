#include "sftp_server.h"
#include "sftp_client.h"
#include "../utils/StdHeader.h"
#include "../utils/Utility.h"

#ifdef _DEBUG
	#pragma comment(lib, "sftpD")
	#pragma comment(lib, "utilsD")
#else
	#pragma comment(lib, "sftp")
	#pragma comment(lib, "utils")
#endif

static int svr_download_finished(void* priv, const char* file)
{
	printf("文件%s已下载到客户端\n", file);
	return 0;
}

static int svr_upload_finished(void* priv, const char* file)
{
	printf("上传结束，保存到本地文件%s\n", file);
	return 0;
}

int main(int argc, char** argv)
{
	if(argv[1] == NULL)
	{
		printf("Usage(for server): %s server [svr_ip] [svr_port] [max_size]\n",argv[0]);
		printf("Usage(for client): %s client [svr_ip] [svr_port] [remote_file] [local_file] [download|upload]\n", argv[0]);
		return 0;
	}

#ifdef _WIN32
	WORD wVersion=MAKEWORD(2,2);
	WSADATA wsData;
	if (WSAStartup(wVersion,&wsData) != 0)  return -1;
#endif

	if(strcmp(argv[1], "server") == 0)
	{
		if(argc != 5)
		{
			printf("Usage: %s server [svr_ip] [svr_port] [max_size]\n", argv[0]);
			return 0;
		}
		SFTP_SVR* h = sftp_server_init(argv[2], atoi(argv[3]));
		assert(h != NULL);
		sftp_server_setopt(h, SFTPOPT_DOWNLOAD_FINISHED_FUNCTION, svr_download_finished);
		sftp_server_setopt(h, SFTPOPT_UPLOAD_FINISHED_FUNCTION, svr_upload_finished);
		sftp_server_setopt(h, SFTPOPT_MAX_DATA_PACKET_SIZE, atoi(argv[4]));
		sftp_server_start(h);
		while(!KeyboardHit('q'))
		{
			Sleep(100);
		}
		sftp_server_stop(h);
		sftp_server_exit(h);
	}
	else if(strcmp(argv[1], "client") == 0)
	{
		if(argc != 7)
		{
			printf("Usage: %s client [svr_ip] [svr_port] [remote_file] [local_file] [download|upload]\n", argv[0]);
			return 0;
		}

		if(strcmp(argv[6], "download") == 0) //下载文件
		{
			time_t t1 = time(0);
			int c = sftp_client_download_file(argv[2], atoi(argv[3]), argv[4], argv[5]);
			if(c == SFTP_CODE_OK)
			{
				printf("下载完成，用时%d秒\n", time(0) - t1);
			}
			else
			{
				printf("下载失败！返回码： %d\n", c);
			}
			
		}
		else if(strcmp(argv[6], "upload") == 0) //上传文件
		{
			time_t t1 = time(0);
			int c = sftp_client_upload_file(argv[2], atoi(argv[3]), argv[5], argv[4]);
			if(c == SFTP_CODE_OK)
			{
				printf("上传完成，用时%d秒\n", time(0) - t1);
			}
			else
			{
				printf("上传失败！返回码： %d\n", c);
			}
		}
	}

#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}
