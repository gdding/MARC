#include "Utility.h"
#include "kbhit.h"
#include "DirScanner.h"
#include "AppRunner.h"


bool Exec(const char* cmd)
{
	return Exec(cmd, -1);
}

bool Exec(const char* cmd, int nTimeout)
{
	string sCommand(cmd);
#if !RUN_APP_WITH_FORK
	int nCmdRet = system(sCommand.c_str());
	return true;
#else
	CAppRunner runner;
	time_t nStartTime = time(0);
	if(!runner.ExecuteApp(sCommand.c_str()))
	{
		return false;
	}
	while(runner.IsAppRunning())
	{
		if(nTimeout > 0 && time(0) - nStartTime > nTimeout)
		{
			runner.KillApp();
			break;
		}
		Sleep(200);
	}
	int nAppExitCode = runner.GetAppExitCode();
	switch(nAppExitCode)
	{
	case APP_EXIT_CODE_TIMEOUT:
		return false;
	case APP_EXIT_CODE_ABORT:
		return false;
	default:
		return true;
	};
#endif
}

string getFilePath(const char* sFilename)
{
	string sFilePath = "";
	
	int nFilenameLen = (int)strlen(sFilename);
	if(nFilenameLen == 0) return sFilePath;
	int nEnd = nFilenameLen - 1;
	while(nEnd > 0)
	{
		if(sFilename[nEnd] == '/' || sFilename[nEnd] == '\\')
		{
			break;
		}
		nEnd--;
	}

	for(int i = 0; i <= nEnd; i++)
	{
		sFilePath += sFilename[i];
	}

	return sFilePath;
}

void NormalizePath(char* dir, bool bTail)
{
	size_t len = strlen(dir);
#ifdef _WIN32
	for(size_t m=0; m<len; m++)
	{
		if(dir[m]=='/') dir[m]='\\';
	}
	if(bTail)
	{
		if(dir[len-1] != '\\')
		{
			dir[len] = '\\';
			dir[len+1] = 0;
		}
	}
#else
	for(size_t m=0; m<len; m++)
	{
		if(dir[m]=='\\') dir[m]='/';
	}
	if(bTail)
	{
		if(dir[len-1] != '/')
		{
			dir[len] = '/';
			dir[len+1] = 0;
		}
	}
#endif
}

void NormalizePath(string& dir, bool bTail)
{
	size_t len = dir.length();
#ifdef _WIN32
	for(size_t m=0; m<len; m++)
	{
		if(dir[m]=='/') dir[m]='\\';
	}
	if(bTail)
	{
		if(dir[len-1] != '\\')
		{
			dir += '\\';
		}
	}
#else
	for(size_t m=0; m<len; m++)
	{
		if(dir[m]=='\\') dir[m]='/';
	}
	if(bTail)
	{
		if(dir[len-1] != '/')
		{
			dir += '/';
		}
	}
#endif
}

bool CreateFilePath(const char* path)
{
	if(path==NULL || path[0]==0)  return false;

	string sCurPath("");
	int i = 0;
	char ch = path[i];
	while(ch != 0)
	{
		sCurPath += ch;
		if(ch == '/' || ch == '\\')
		{
			if(!DIR_EXIST(sCurPath.c_str()) && (MAKE_DIR(sCurPath.c_str()) != 0))
			{
				return false;
			}
		}
		ch = path[++i];
	}
	return true;
}

bool KeyboardHit(char ch)
{
	if (_kbhit())
	{
		if (_getch() == ch)
		{
			return true;
		}
	}
	return false;
}

bool KeyboardHit(const string& s)
{
	string sHit;
	while(_kbhit())
	{
		sHit += _getch();
	}
	if(!sHit.empty())
	{
		printf("You pressed: %s\n", sHit.c_str());
		if(sHit.find(s) != string::npos) return true;
	}
	return false;
}

bool CreateFlagFile(const char* filepath)
{
	FILE *fp = fopen(filepath, "wb");
	if(fp == NULL) return false;
	fclose(fp);
	return true;
}

void deleteFile(const char* file)
{
	if(file == NULL || file[0] == 0) return ;
	char chDelCmd[1024] = {0};
#ifdef _WIN32
	sprintf(chDelCmd, "DEL %s /F /Q", file);
#else
	sprintf(chDelCmd, "rm -rf %s", file);
#endif
	system(chDelCmd);
}

void deleteDir(const char* path)
{
	char chDelCmd[1024] = {0};
#ifdef _WIN32
	sprintf(chDelCmd, "rd /S /Q %s", path);
#else
	sprintf(chDelCmd, "rm -rf %s", path);
#endif
	system(chDelCmd);
}

void CleanDir(const char* path)
{
	char chCmd[1024] = {0};
#ifdef _WIN32
	char s[1024] = {0};
	strcpy(s, path);
	NormalizePath(s);
	RMDIR(s);
	sprintf(chCmd, "del %s*.* /s/f/q", s);
#else
	sprintf(chCmd, "rm -rf %s*", path);
#endif
	system(chCmd);
}

#ifdef _WIN32
void RMDIR(const char *pPath)
{
	CDirScanner ds(pPath);
	const vector<string>& dirs = ds.GetDirList();
	for(size_t i=0; i<dirs.size(); i++)
	{
		char chCmd[1024] = {0};
		sprintf(chCmd, "rd /S /Q %s%s", pPath, dirs[i].c_str());
		system(chCmd);
	}
}
#endif

void SplitCmdStringBySpace(const char* str, vector<string>& result)
{
	bool bQuotationMark = false;
	string s = "";
	for(int i = 0; str[i] != 0; i++)
	{
		char c = str[i];
		if(c == '"')
		{
			bQuotationMark = !bQuotationMark;
		}
		if(!bQuotationMark)
		{
			if(c == ' ' || c == '\t')
			{
				if(!s.empty()) result.push_back(s);
				s = "";
				continue;
			}
		}
		s += c;
	}
	if(!s.empty()) result.push_back(s);
}

//从文本文件中读取key/value信息存入数组
//每行格式：[KEY]\t[VALUE]
//KEY为字符串，VALUE为整数
//返回读取成功的行数，失败返回-1
int ReadKeyValues(const char* sFileName, vector<pair<string,int> >& result)
{
	FILE *fp = fopen(sFileName, "rb");
	if(fp == NULL) return -1;
	char sLine[1024];
	while (fgets(sLine, 1024, fp))
	{
		char sKey[512] = "";
		int nValue = 0;
		if(sscanf(sLine,"%s %d", sKey, &nValue) != 2) continue;
		if (sKey[0] == 0) continue;
		result.push_back(make_pair(sKey, nValue));
	}
	fclose(fp);
	return (int)result.size();
}
int ReadKeyValues(const char* sFileName, vector<pair<string,string> >& result)
{
	FILE *fp = fopen(sFileName, "rb");
	if(fp == NULL) return -1;
	char sLine[1024];
	while (fgets(sLine, 1024, fp))
	{
		char sKey[512] = {0};
		char sValue[512] = {0};
		if(sscanf(sLine,"%s %s", sKey, sValue) != 2) continue;
		if (sKey[0] == 0) continue;
		result.push_back(make_pair(sKey, sValue));
	}
	fclose(fp);
	return (int)result.size();
}

string formatDateTime(time_t nTime, int nFormat)
{
	struct tm stTime = *localtime(&nTime);
	char sBuffer[256] = {0};
	switch (nFormat)
	{
	case 0:
		_snprintf(sBuffer, 
			sizeof(sBuffer),
			"%04d-%02d-%02d %02d:%02d:%02d", 
			stTime.tm_year+1900,
			stTime.tm_mon+1,
			stTime.tm_mday,
			stTime.tm_hour,
			stTime.tm_min,
			stTime.tm_sec);
		break;
	case 1:
		_snprintf(sBuffer, 
			sizeof(sBuffer),
			"%04d%02d%02d%02d%02d%02d", 
			stTime.tm_year+1900,
			stTime.tm_mon+1,
			stTime.tm_mday,
			stTime.tm_hour,
			stTime.tm_min,
			stTime.tm_sec);
		break;
	case 2:
		_snprintf(sBuffer, 
			sizeof(sBuffer),
			"%04d%02d%02d", 
			stTime.tm_year+1900,
			stTime.tm_mon+1,
			stTime.tm_mday);
		break;
	default:
		break;
	};

	return string(sBuffer);
}

int getCurDate()
{
	struct tm* ptime;
	time_t now;
	time(&now);
	ptime = localtime(&now);
	int nDate= (ptime->tm_year+1900)*10000 + (ptime->tm_mon+1)*100 + ptime->tm_mday;
	return nDate;
}

int getFileSize(const char* sFilename)
{
	int nFileSize = -1;
	FILE *fp = fopen(sFilename,"rb");
	if (fp != NULL)
	{
		fseek(fp, 0, SEEK_END);
		nFileSize = ftell(fp);
		fclose(fp);
	}
	return nFileSize;
}

string getFileName(const string& sFilePath, bool bExtension)
{
	string sFileName = "";
	int i = (int)sFilePath.length() - 1;
	for(; i >= 0; i--)
	{
		if(sFilePath[i] == '/' || sFilePath[i] == '\\') break;
	}
	for(int j = i+1; j < (int)sFilePath.length(); j++)
	{
		sFileName += sFilePath[j];
	}
	if(!bExtension)
	{
		string::size_type pos = sFileName.rfind(".");
		if(pos != string::npos)
		{
			sFileName = string(sFileName.c_str(), pos);
		}
	}

	return sFileName;
}


int readFile(const char *sFilename, char* & pBuffer)
{
	FILE *fp = fopen(sFilename,"rb");
	if (fp == NULL) return -1;
	fseek(fp, 0, SEEK_END);
	int nFileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
    pBuffer = (char*)calloc(nFileSize+1, sizeof(char));
	if(pBuffer == NULL)
	{
		fclose(fp);
		return -1;
	}
	fread(pBuffer, 1, nFileSize, fp);
	pBuffer[nFileSize]=0;
    fclose(fp);
	return nFileSize;
}

int readFile(const char *sFilename, int nOffset, char* pBuffer, int nBufSize)
{
	int nReadSize = 0;
	FILE *fp = fopen(sFilename, "rb");
	if (fp != NULL)
	{
		fseek(fp, 0, SEEK_END);
		int nFileSize = ftell(fp);
		if(nOffset < nFileSize)
		{
			nReadSize = (nFileSize-nOffset<nBufSize ? nFileSize-nOffset:nBufSize);
			fseek(fp, nOffset, SEEK_SET);
			fread(pBuffer, 1, nReadSize, fp);
		}
		fclose(fp);
	}
	return nReadSize;
}


string GenRandomString(int n)
{
    static unsigned int seed=1;
    srand((unsigned int)time(0) + (seed++));
    string s;
    const int RANDOM_SET_SIZE = 36;
	static char sChars[]="0123456789abcdefghijklmnopqrstuvwxyz";
    for(int i=0; i < n; i++)
    {
	    s += sChars[rand()%RANDOM_SET_SIZE];
    }
    return s;
}

