/*-----------------------------------------------------------------------------
* Copyright (c) 2010 ICT, CAS. All rights reserved.
*   yangshutong@software.ict.ac.cn
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.

* Last-Modified: 2010-02-04
*-----------------------------------------------------------------------------*/
#ifndef INI_FILE_CPP_H_
#define INI_FILE_CPP_H_

#include <string>
using namespace std;

class IniFile
{
public:
	IniFile(const string & fileName);
public:
	virtual ~IniFile(void);

	const string & getFileName() const;

	const string &getSection() const;
	void setSection(const string &section);

	bool write(const string &key, const string & value) const ;
	bool write(const string &key, int value) const ;
	bool write(const string &key, long value) const ;

	bool readStr(const string &key,string &value) const;
	bool readInt(const string &key, int &value) const;
	bool readLong(const string &key, long &value) const;

public:
	static int read_profile_string( const char *section, const char *key,char *value, int size, const char *default_value, const char *file);
	static bool read_profile_int( const char *section, const char *key,int &ivalue, const char *file);
	static bool read_profile_long( const char *section, const char *key,long &ivalue, const char *file);
	static int write_profile_string(const char *section, const char *key,const char *value, const char *file);

private:
	static int load_ini_file(const char *file, char *buf,int *file_size);
	static int newline(char c);
	static int end_of_string(char c);
	static int left_barce(char c);
	static int right_brace(char c );
	static int parse_file(const char *section, const char *key, const char *buf,int *sec_s,int *sec_e,int *key_s,int *key_e, int *value_s, int *value_e);

private:
	string m_fileName;
	string m_section;
};



#endif


