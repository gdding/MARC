/*-----------------------------------------------------------------------------
* Copyright (c) 2010 ICT, CAS. All rights reserved.
*   yangshutong@software.ict.ac.cn
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.

* Last-Modified: 2010-02-04
*-----------------------------------------------------------------------------*/
#ifndef __MYINIFILE_H__
#define __MYINIFILE_H__
#include "IniFile.h"

namespace INI
{
	class CMyIniFile
	{
	public:
		static bool ReadIniStr(const string filename,const string Section,const string key,string &value);
		static bool ReadIniInt(const string filename,const string Section,const string key,int &value);
		static bool ReadIniLong(const string filename,const string Section,const string key,unsigned long &value);
		static bool WriteIniStr(const string filename,const string Section,const string key,string value);
		static bool WriteIniInt(const string filename,const string Section,const string key,int value);
		static bool WriteIniLong(const string filename,const string Section,const string key,long value);
	};
}

#endif //__MYINIFILE_H__



