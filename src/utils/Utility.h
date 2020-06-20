/*-----------------------------------------------------------------------------
* Copyright (c) 2010 ICT, CAS. All rights reserved.
*   dingguodong@software.ict.ac.cn
*   yangshutong@software.ict.ac.cn
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.

* Last-Modified: 2010-02-04
*-----------------------------------------------------------------------------*/
#ifndef _H_UTILITY_GDDING_INCLUDED_20100204
#define _H_UTILITY_GDDING_INCLUDED_20100204
#include "StdHeader.h"

//执行命令
bool Exec(const char* cmd);
bool Exec(const char* cmd, int nTimeout);

//根据给定的文件名（含路径）获得其路径
string getFilePath(const char* sFilename);

//路径规范化
void NormalizePath(char* dir, bool bTail=true);
void NormalizePath(string& dir,  bool bTail=true);

//创建文件路径
bool CreateFilePath(const char* path);

//判断是否按下了某个键
bool KeyboardHit(char ch);

//判断是否连续按下了某个字符串
bool KeyboardHit(const string& s);

//创建一个空的标志文件
bool CreateFlagFile(const char* filepath);

//清空目录
void CleanDir(const char* path);

//删除目录
void deleteDir(const char* path);

//删除文件
void deleteFile(const char* file);

//从文本文件中读入键值对
int ReadKeyValues(const char* sFileName, vector<pair<string,int> >& result);
int ReadKeyValues(const char* sFileName, vector<pair<string,string> >& result);

//获得系统日期,结果形式为YYYY-MM-DD hh:mm:ss
string getDateTime();

//将time_t数字型时间转换为字符串型，支持的格式
//0: YYYY-MM-DD hh:mm:ss（如2010-02-04 13:53:02）
//1: YYYYMMDDhhmmss（如20100204135302）
//2: YYYYMMDD
string formatDateTime(time_t nTime, int nFormat=0);

//获得系统日期,结果形式为YYYYMMDD
int getCurDate();

//获得文件大小
int getFileSize(const char* sFilename);

//根据文件路径取出文件名（bExtension为false时不含扩展名）
string getFileName(const string& sFilePath, bool bExtension=false);

//生成长为n的随机字符串
string GenRandomString(int n);

//从文件的指定偏移开始最多读取nBufSize个字节到缓冲区pBuffer,
//返回：实际读取的字节数
//			-1 - 文件不存在
int readFile(const char *sFilename, int nOffset, char* pBuffer, int nBufSize);


//读取文件数据到缓冲区pBuffer
//返回：-1 － 文件不存在
//     >=0  － 读取成功
int readFile(const char *sFilename, char* & pBuffer);

//以空字符(' ', '\t', '\r', '\n)作为分隔将命令行字符串s切分成子串存入result
//需考虑双引号中的空格
void SplitCmdStringBySpace(const char* str, vector<string>& result);


#ifdef _WIN32
void RMDIR(const char *pPath);
#endif

#endif //_H_UTILITY_GDDING_INCLUDED_20100204

