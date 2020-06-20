/*-----------------------------------------------------------------------------
* Copyright (c) 2010 ICT, CAS. All rights reserved.
*   dingguodong@software.ict.ac.cn
*   yangshutong@software.ict.ac.cn
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.

* Last-Modified: 2010-03-05
*-----------------------------------------------------------------------------*/
#ifndef _H_MYZIP_GDDING_INCLUDED_20100304
#define _H_MYZIP_GDDING_INCLUDED_20100304
#include <string>
#include <vector>
using std::string;
using std::vector;

class CMyZip
{
public:
	CMyZip(void);
	virtual ~CMyZip(void);

public:
	//将目录sInDir中的所有文件及文件夹压缩到文件sZipFile
	bool zip(const char* sInDir, const char* sZipFile);

	//将文件sZipFile解压到目录sOutDir;
	bool unzip(const char *sZipFile, const char *sOutDir);

private:
	int Combine2TempFile(const vector<string>& oFilePaths, const string& sBaseDir, const string& sTempFile);
	bool XCombineTempFile(const char* sTempFile, const char* sOutPath);
};


#endif // _H_MYZIP_GDDING_INCLUDED_20100304


