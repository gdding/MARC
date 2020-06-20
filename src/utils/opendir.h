/*-----------------------------------------------------------------------------
* Copyright (c) 2010 ICT, CAS. All rights reserved.
*   dingguodong@software.ict.ac.cn
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.

* Last-Modified: 2010-02-04
*-----------------------------------------------------------------------------*/
#ifndef _H_OPENDIR_DINGLIN_INCLUDED
#define _H_OPENDIR_DINGLIN_INCLUDED

#include "StdHeader.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32   

	struct dirent
	{   
		long d_ino;   
		off_t d_off;     
		unsigned short d_reclen;   
		char d_name[_MAX_FNAME+1];   
	};   

	typedef struct
	{   
		intptr_t handle;                               
		short offset;                             
		short finished;                         
		struct _finddata_t fileinfo;   
		char *dir;                                       
		struct dirent dent;                     
	} DIR;   

	DIR* opendir(const char* );   
	struct dirent* readdir(DIR* );   
	int closedir(DIR* );  

#endif


#ifdef __cplusplus
}
#endif



#endif //_H_OPENDIR_DINGLIN_INCLUDED
