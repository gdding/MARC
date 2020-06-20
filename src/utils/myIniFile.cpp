#include "myIniFile.h"
using namespace INI;

bool CMyIniFile::ReadIniStr(const string filename,const string Section,const string key,string &value)
{
	IniFile ini(filename);
	ini.setSection(Section);
	return ini.readStr(key,value);
}
bool CMyIniFile::ReadIniInt(const string filename,const string Section,const string key,int &value)
{
	IniFile ini(filename);
	ini.setSection(Section);
	return ini.readInt(key,value);
}

bool CMyIniFile::ReadIniLong(const string filename,const string Section,const string key,unsigned long &value)
{
	IniFile ini(filename);
	ini.setSection(Section);
	long lValue=0;
	if(ini.readLong(key,lValue))
	{
		value = lValue;
		return true;
	}
	else
		return false;
}

bool CMyIniFile::WriteIniStr(const string filename,const string Section,const string key,string value)
{
	IniFile ini(filename);
	ini.setSection(Section);
	if(!ini.write(key,value))
	{
		return false;
	}
	return true;
}

bool CMyIniFile::WriteIniInt(const string filename,const string Section,const string key,int value)
{
	IniFile ini(filename);
	ini.setSection(Section);
	if(!ini.write(key,value))
	{
		return false;
	}
	return true;
}

bool CMyIniFile::WriteIniLong(const string filename,const string Section,const string key,long value)
{
	IniFile ini(filename);
	ini.setSection(Section);
	if(!ini.write(key,value))
	{
		return false;
	}
	return true;
}
