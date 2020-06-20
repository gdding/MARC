/*-----------------------------------------------------------------------------
* Copyright (c) 2010 ICT, CAS. All rights reserved.
*   dingguodong@software.ict.ac.cn
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.

* Last-Modified: 2010-03-04
*-----------------------------------------------------------------------------*/
#ifndef _H_DIRSCANNER_GDDING_INCLUDED_20100304
#define _H_DIRSCANNER_GDDING_INCLUDED_20100304
#include "StdHeader.h"

class CDirScanner
{
public:
	CDirScanner(const char* dir, bool bRecursive=true);

public:
	//获得被扫描的目录
	const string& GetBaseDir(){return m_sBaseDir;}

	//获得该目录下扫描到的文件相对路径列表（不含该目录名）
	const vector<string>& GetFileList(){return m_vFileList;}

	//获得该目录下扫描到的文件夹相对路径列表（不含该目录名）
	const vector<string>& GetDirList(){return m_vDirList;}

	//获得该目录下扫描到的所有相对路径列表（不含该目录名）
	const vector<string>& GetAllList(){return m_vAllList;}

	//获得该目录下扫描到的文件完整路径列表（包含该目录名）
	const vector<string>& GetFileFullList(){return m_vFileFullList;}

protected:
	void perform(const char *dir, bool bRecursive=true);
	virtual void OnFileFound(const char *filename);
	virtual void OnSubDirFound(const char *currentdir, const char *parentdir);

private:
	vector<string> m_vFileList;	//文件相对路径列表
	vector<string> m_vDirList;	//文件夹相对路径列表
	vector<string> m_vAllList;	//所有的列表（含文件和文件夹）
	vector<string> m_vFileFullList;//文件完整路径列表

private:
	string m_sBaseDir;
	int m_bRecursive;
};

#endif //_H_DIRSCANNER_GDDING_INCLUDED_20100304
