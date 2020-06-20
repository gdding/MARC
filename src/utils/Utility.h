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

//ִ������
bool Exec(const char* cmd);
bool Exec(const char* cmd, int nTimeout);

//���ݸ������ļ�������·���������·��
string getFilePath(const char* sFilename);

//·���淶��
void NormalizePath(char* dir, bool bTail=true);
void NormalizePath(string& dir,  bool bTail=true);

//�����ļ�·��
bool CreateFilePath(const char* path);

//�ж��Ƿ�����ĳ����
bool KeyboardHit(char ch);

//�ж��Ƿ�����������ĳ���ַ���
bool KeyboardHit(const string& s);

//����һ���յı�־�ļ�
bool CreateFlagFile(const char* filepath);

//���Ŀ¼
void CleanDir(const char* path);

//ɾ��Ŀ¼
void deleteDir(const char* path);

//ɾ���ļ�
void deleteFile(const char* file);

//���ı��ļ��ж����ֵ��
int ReadKeyValues(const char* sFileName, vector<pair<string,int> >& result);
int ReadKeyValues(const char* sFileName, vector<pair<string,string> >& result);

//���ϵͳ����,�����ʽΪYYYY-MM-DD hh:mm:ss
string getDateTime();

//��time_t������ʱ��ת��Ϊ�ַ����ͣ�֧�ֵĸ�ʽ
//0: YYYY-MM-DD hh:mm:ss����2010-02-04 13:53:02��
//1: YYYYMMDDhhmmss����20100204135302��
//2: YYYYMMDD
string formatDateTime(time_t nTime, int nFormat=0);

//���ϵͳ����,�����ʽΪYYYYMMDD
int getCurDate();

//����ļ���С
int getFileSize(const char* sFilename);

//�����ļ�·��ȡ���ļ�����bExtensionΪfalseʱ������չ����
string getFileName(const string& sFilePath, bool bExtension=false);

//���ɳ�Ϊn������ַ���
string GenRandomString(int n);

//���ļ���ָ��ƫ�ƿ�ʼ����ȡnBufSize���ֽڵ�������pBuffer,
//���أ�ʵ�ʶ�ȡ���ֽ���
//			-1 - �ļ�������
int readFile(const char *sFilename, int nOffset, char* pBuffer, int nBufSize);


//��ȡ�ļ����ݵ�������pBuffer
//���أ�-1 �� �ļ�������
//     >=0  �� ��ȡ�ɹ�
int readFile(const char *sFilename, char* & pBuffer);

//�Կ��ַ�(' ', '\t', '\r', '\n)��Ϊ�ָ����������ַ���s�зֳ��Ӵ�����result
//�迼��˫�����еĿո�
void SplitCmdStringBySpace(const char* str, vector<string>& result);


#ifdef _WIN32
void RMDIR(const char *pPath);
#endif

#endif //_H_UTILITY_GDDING_INCLUDED_20100204

