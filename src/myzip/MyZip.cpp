#include "MyZip.h"
#include <zlib.h>
#include <assert.h>
#include "../utils/Utility.h"
#include "../utils/DirScanner.h"
#include "../utils/RunLogger.h"

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#	include <fcntl.h>
#	include <io.h>
#	define SET_BINARY_MODE(file) setmode(fileno(file),O_BINARY)
#else 
#	define SET_BINARY_MODE(file)
#endif

#define MYZIP_BUFSIZE			(512*1024) //512KB buffer
#define INVALID_PARAM		-128

#pragma comment(lib, "zdll")
static CRunLogger Logger("./log/marc_myzip.log", true);

static int myzip(const char *srcFile, const char *dstFile, int level = Z_DEFAULT_COMPRESSION);
static int myunzip(const char *srcFile, const char *dstFile);
static void zerror(int ret);

static int AppendFile(FILE *fpSrc, FILE *fpDst);
static int AppendFile(const char *src, FILE *fpDst);
static int ExtractFile(FILE *fpSrc, FILE *fpDst, int size);

CMyZip::CMyZip(void)
{
}

CMyZip::~CMyZip(void)
{
}

bool CMyZip::zip(const char* sDir, const char* sZipFile)
{
	assert(sDir != NULL);
	assert(sZipFile != NULL);

	char sTempFile[50] = {0};
	int nTempFileSize = 0;
	char* pInBuffer = NULL;
	char* pOutBuffer = NULL;
	unsigned long nInSize = 0;
	unsigned long nOutSize = 0;
	int nRetCode = 0;
	FILE* fpZipFile = NULL;
	char chCmd[1024] = {0};

	//遍历该目录
	CDirScanner ds(sDir);
	const string& sBaseDir = ds.GetBaseDir();
	const vector<string>& oFilePaths = ds.GetAllList();
	if(oFilePaths.empty())
	{
		Logger.Write(CRunLogger::LOG_ERROR, "Empty directory: %s, %s:%d\n", sBaseDir.c_str(), __FILE__, __LINE__);
		goto _FAILURE;
	}

	//合并到临时文件
	sprintf(sTempFile, "myzip_%s.tmp", GenRandomString(10).c_str());
	nTempFileSize = Combine2TempFile(oFilePaths, sBaseDir, sTempFile);
	if(nTempFileSize < 0)
	{
		goto _FAILURE;
	}

	//压缩临时文件
	nRetCode = myzip(sTempFile, sZipFile);
	if(nRetCode != Z_OK)
	{
		zerror(nRetCode);
		goto _FAILURE;
	}

	//删除临时文件
	deleteFile(sTempFile);
	return true;
_FAILURE:
	if(sTempFile[0] != 0)
	{
		deleteFile(sTempFile);
	}
	return false;
}

bool CMyZip::unzip(const char *sZipFile, const char *sOutDir)
{
	assert(sZipFile != NULL);
	assert(sOutDir != NULL);

	char sTempFile[50] = {0};
	int nRetCode = 0;

	//解压，写入临时文件
	sprintf(sTempFile, "myzip_%s.tmp", GenRandomString(10).c_str());
	nRetCode = myunzip(sZipFile, sTempFile);
	if(nRetCode != Z_OK) 
	{
		zerror(nRetCode);
		goto _FAILURE;
	}

	//拆分解压后的临时文件
	if(!XCombineTempFile(sTempFile, sOutDir))
	{
		goto _FAILURE;
	}

	//删除临时文件
	deleteFile(sTempFile);

	Logger.Write(CRunLogger::LOG_INFO, "unzip the file %s to dir %s successfully\n", sZipFile, sOutDir);
	return true;
_FAILURE:
	if(sTempFile[0] != 0)
	{
		deleteFile(sTempFile);
	}
	return false;
}

int CMyZip::Combine2TempFile(const vector<string>& oFilePaths, const string &sBaseDir, const string& sTempFile)
{
	FILE *fpTemp = fopen(sTempFile.c_str(), "wb");
	if(fpTemp == NULL)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "Failed to create temp file \"%s\", %s:%d\n", sTempFile.c_str(), __FILE__, __LINE__);
		return -1;
	}

	//将所有文件及目录写入临时文件
	size_t nTempFileSize = 0;
	for(size_t i=0; i < oFilePaths.size(); i++)
	{
		const string& sFilePath = oFilePaths[i];
		char chBuf[1024] = {0};
		size_t nWriteSize = 0;
		if(sFilePath[sFilePath.length()-1]=='/' || sFilePath[sFilePath.length()-1]=='\\') //是一个文件夹
		{
			sprintf(chBuf, "%-8d%s%-16u", sFilePath.length(), sFilePath.c_str(), 0);
			nWriteSize = fwrite(chBuf, sizeof(char), strlen(chBuf), fpTemp);
			assert(nWriteSize == strlen(chBuf));
			nTempFileSize += nWriteSize;
			continue;
		}
		
		//是一个文件
		string sRegFilePath = sBaseDir + sFilePath;
		int nFileSize = getFileSize(sRegFilePath.c_str());
		if(nFileSize < 0)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "Read file error: %s, %s:%d\n", sRegFilePath.c_str(), __FILE__, __LINE__);
			fclose(fpTemp);
			fpTemp = NULL;
			return -1;
		}
		sprintf(chBuf, "%-8d%s%-16u", sFilePath.length(), sFilePath.c_str(), nFileSize);
		nWriteSize = fwrite(chBuf, sizeof(char), strlen(chBuf), fpTemp);
		if(nWriteSize != strlen(chBuf))
		{
			Logger.Write(CRunLogger::LOG_ERROR, "exception happened when write file: %s, %s:%d\n", sTempFile.c_str(), __FILE__, __LINE__);
			fclose(fpTemp);
			fpTemp = NULL;
			return -1;
		}
		nTempFileSize += nWriteSize;
		nWriteSize = AppendFile(sRegFilePath.c_str(), fpTemp);
		if(nWriteSize != nFileSize)
		{
			Logger.Write(CRunLogger::LOG_ERROR, "exception happened when write file: %s, %s:%d\n", sTempFile.c_str(), __FILE__, __LINE__);
			fclose(fpTemp);
			fpTemp = NULL;
			return -1;
		}
		nTempFileSize += nFileSize;
	}
	fclose(fpTemp);
	fpTemp = NULL;
	return (int)nTempFileSize;
}

bool CMyZip::XCombineTempFile(const char* sTempFile, const char* sOutPath)
{
	FILE* fpTemp= fopen(sTempFile, "rb");
	if(fpTemp == NULL)
	{
		Logger.Write(CRunLogger::LOG_ERROR, "can't open the temp file %s, %s:%d\n", sTempFile, __FILE__, __LINE__);
		return false;
	}

	while(!feof(fpTemp))
	{
		char chBuf[1025] = {0};
		
		//读取路径大小
		fread(chBuf, sizeof(char), 8, fpTemp);
		int nFilePathSize = atoi(chBuf);
		if(nFilePathSize == 0) continue;
			
		//读取文件或文件夹的路径
		memset(chBuf, 0, 1025);
		fread(chBuf, sizeof(char), nFilePathSize, fpTemp);
		string sRegOutPath(sOutPath);
		NormalizePath(sRegOutPath);
		string sFilePath = sRegOutPath + chBuf;
		NormalizePath(sFilePath, false);
		Logger.Write(CRunLogger::LOG_INFO, "create %s\n", sFilePath.c_str());

		//创建该路径
		if(!CreateFilePath(sFilePath.c_str()))
		{
			Logger.Write(CRunLogger::LOG_ERROR, "Failed to create the file path: %s, %s:%d\n", sFilePath.c_str(), __FILE__, __LINE__);
			fclose(fpTemp);
			fpTemp = NULL;
			return false;
		}

		//读取文件大小
		memset(chBuf, 0, 1025);
		fread(chBuf, sizeof(char), 16, fpTemp);
		int nFileSize = atoi(chBuf);
		assert(nFileSize >= 0);

		//非文件夹则创建该文件并写入数据
#ifdef _WIN32
		if(sFilePath[sFilePath.length()-1] != '\\')
#else
		if(sFilePath[sFilePath.length()-1] != '/')
#endif
		{
			FILE* fp = fopen(sFilePath.c_str(), "wb");
			if(fp == NULL)
			{
				Logger.Write(CRunLogger::LOG_ERROR, "can't create the file: %s, %s:%d\n", sFilePath.c_str(), __FILE__, __LINE__);
				fclose(fpTemp);
				fpTemp = NULL;
				return false;
			}

			if(nFileSize > 0)
			{
				int nReadSize = ExtractFile(fpTemp, fp, nFileSize);
				if(nReadSize != nFileSize)
				{
					Logger.Write(CRunLogger::LOG_ERROR, "exception happened when Extract file %s, %s:%d\n", sTempFile, __FILE__, __LINE__);
					fclose(fpTemp);
					fpTemp = NULL;
					return false;
				}
			}
			fclose(fp);
			fp = NULL;
		}
	}
	fclose(fpTemp);
	fpTemp = NULL;
	return true;
}

static
int myzip(const char *srcFile, const char *dstFile, int level)
{
	if(srcFile == NULL || dstFile == NULL ||
	   *srcFile == '\0' || *dstFile == '\0')
	   return INVALID_PARAM;
	
	FILE *fpIn = fopen(srcFile, "rb");
	FILE *fpOut = fopen(dstFile, "wb");

	if(fpIn == NULL || fpOut == NULL)
		return INVALID_PARAM;

	int ret = Z_OK, flush;
	unsigned have;
	z_stream strm;
	unsigned char *in = (unsigned char*)calloc(MYZIP_BUFSIZE, sizeof(unsigned char));
	unsigned char *out = (unsigned char*)calloc(MYZIP_BUFSIZE, sizeof(unsigned char));
	if(in == NULL || out == NULL)
	{
		fclose(fpIn);
		fclose(fpOut);
		return Z_MEM_ERROR;
	}

	SET_BINARY_MODE(fpIn);
	SET_BINARY_MODE(fpOut);

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;

	ret = deflateInit(&strm, level);
	if(ret != Z_OK)
		goto FREE;

	do
	{
		strm.avail_in = fread(in, sizeof(unsigned char), MYZIP_BUFSIZE, fpIn);
		if(ferror(fpIn)) 
		{
			deflateEnd(&strm);
			ret = Z_ERRNO;
			goto __OUT__;
		}
		flush = feof(fpIn) ? Z_FINISH : Z_NO_FLUSH;
		strm.next_in = in;
		
		do
		{
			strm.avail_out = MYZIP_BUFSIZE;
			strm.next_out = out;
			ret = deflate(&strm, flush);
			if(ret == Z_STREAM_ERROR)
				goto __OUT__;
			have = MYZIP_BUFSIZE - strm.avail_out;
			if(fwrite(out, sizeof(unsigned char), have, fpOut) != have || ferror(fpOut))
			{
				ret = Z_ERRNO;
				goto __OUT__;
			}
		} while(strm.avail_out == 0);
	} while(flush != Z_FINISH);
	if(ret == Z_STREAM_END)
		ret = Z_OK;

__OUT__:
	deflateEnd(&strm);
FREE:
	free(in);
	free(out);
	fclose(fpIn);
	fclose(fpOut);
	return ret;
}

static
int myunzip(const char *srcFile, const char *dstFile)
{
	if(srcFile == NULL || dstFile == NULL ||
		*srcFile == '\0' || *dstFile == '\0')
		return INVALID_PARAM;

	FILE *fpIn, *fpOut;
	fpIn = fopen(srcFile, "rb");
	fpOut = fopen(dstFile, "wb");
	if(fpIn == NULL || fpOut == NULL)
		return INVALID_PARAM;

	int ret = Z_OK;
	unsigned have;
	z_stream strm;

	unsigned char *in = (unsigned char*)calloc(MYZIP_BUFSIZE, sizeof(unsigned char));
	unsigned char *out = (unsigned char*)calloc(MYZIP_BUFSIZE, sizeof(unsigned char));
	if(in == NULL || out == NULL)
	{
		fclose(fpIn);
		fclose(fpOut);
		return Z_MEM_ERROR;
	}

	SET_BINARY_MODE(fpIn);
	SET_BINARY_MODE(fpOut);

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;

	ret = inflateInit(&strm);
	if(ret != Z_OK)
		goto FREE;

	do
	{
		strm.avail_in = fread(in, sizeof(unsigned char), MYZIP_BUFSIZE, fpIn);
		if(ferror(fpIn))
		{
			ret = Z_ERRNO;
			goto __OUT__;
		}
		if(strm.avail_in == 0)
			break;
		strm.next_in = in;

		do
		{
			strm.avail_out = MYZIP_BUFSIZE;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);
			if(ret == Z_STREAM_ERROR ||
			    ret == Z_NEED_DICT ||
				ret == Z_DATA_ERROR ||
				ret == Z_MEM_ERROR)
				goto __OUT__;
			have = MYZIP_BUFSIZE - strm.avail_out;
			if(fwrite(out, sizeof(unsigned char), have, fpOut) != have || ferror(fpOut))
			{
				ret = Z_ERRNO;
				goto __OUT__;
			}
		} while(strm.avail_out == 0);
	} while(ret != Z_STREAM_END);

	if(ret == Z_STREAM_END)
		ret = Z_OK;
__OUT__:
	inflateEnd(&strm);
FREE:
	free(in);
	free(out);
	fclose(fpIn);
	fclose(fpOut);
	return ret;
}

static void zerror(int ret)
{
	switch(ret) 
	{
	case INVALID_PARAM:
		Logger.Write(CRunLogger::LOG_ERROR, "invalid param for myzip or myunzip\n");
		break;
	case Z_ERRNO:
		Logger.Write(CRunLogger::LOG_ERROR, "Error reading or writing happens...\n");
		break;
	case Z_STREAM_ERROR:
		Logger.Write(CRunLogger::LOG_ERROR, "invalid compression level\n");
		break;
	case Z_DATA_ERROR:
		Logger.Write(CRunLogger::LOG_ERROR, "invalid or incomplete defalte data\n");
		break;
	case Z_MEM_ERROR:
		Logger.Write(CRunLogger::LOG_ERROR, "Out of Memory\n");
		break;
	case Z_VERSION_ERROR:
		Logger.Write(CRunLogger::LOG_ERROR, "zlib version mismatch");
		break;
	default:
		Logger.Write(CRunLogger::LOG_ERROR, "Some unknow error happens\n");
		break;
	}
}

static int AppendFile(const char *src, FILE *fpDst)
{
	int ret = -1;
	FILE *fpSrc = fopen(src, "rb");
	if(fpSrc == NULL)
		return ret;
	ret = AppendFile(fpSrc, fpDst);
	fclose(fpSrc);
	return ret;
}

static int AppendFile(FILE *fpSrc, FILE *fpDst)
{
	int ret = -1, copied = 0;
	if(fpSrc == NULL || fpDst == NULL)
		return ret;

	char *buf = new char[MYZIP_BUFSIZE];
	if(buf == NULL)
		return ret;

	while(!feof(fpSrc))
	{
		ret = fread(buf, sizeof(char), MYZIP_BUFSIZE, fpSrc);
		if(ret < 0 || ferror(fpSrc)) 
		{
			Logger.Write(CRunLogger::LOG_ERROR, "fread failed at %s:%d\n", __FILE__, __LINE__);
			copied = ret;
			goto __OUT__;
		}

		ret = fwrite(buf, sizeof(char), ret, fpDst);
		if(ret < 0 || ferror(fpDst)) 
		{
			Logger.Write(CRunLogger::LOG_ERROR, "fwrite failed at %s:%d\n", __FILE__, __LINE__);
			copied = ret;
			goto __OUT__;
		}

		copied += ret;
	}
__OUT__:
	delete [] buf;
	return copied;
}

static int ExtractFile(FILE *fpSrc, FILE *fpDst, int size)
{
	int ret = -1, copied = 0, remain = size;
	if(fpSrc == NULL || fpDst == NULL)
		return ret;

	char *buf = new char[MYZIP_BUFSIZE];
	if(buf == NULL)
		return ret;

	while(!feof(fpSrc) && remain > 0)
	{
		ret = fread(buf, sizeof(char), 
			MYZIP_BUFSIZE > remain ? remain : MYZIP_BUFSIZE,
			fpSrc);
		if(ret < 0 || ferror(fpSrc))
		{
			Logger.Write(CRunLogger::LOG_ERROR, "fread error at %s:%d\n", __FILE__, __LINE__);
			copied = ret;
			goto __OUT__;
		}

		ret = fwrite(buf, sizeof(char), ret, fpDst);
		if(ret < 0 || ferror(fpDst))
		{
			Logger.Write(CRunLogger::LOG_ERROR, "fwrite error at %s:%d\n", __FILE__, __LINE__);
			copied = ret;
			goto __OUT__;
		}

		copied += ret;
		remain -= ret;
	}
__OUT__:
	delete [] buf;
	return copied;
}