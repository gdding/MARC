/*-----------------------------------------------------------------------------
* Copyright (c) 2010 ICT, CAS. All rights reserved.
*   dingguodong@software.ict.ac.cn
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.
 
* Last-Modified: 2010-03-08
*-----------------------------------------------------------------------------*/
#include <stdio.h>
#include "MyZip.h"

#ifdef _DEBUG
    #pragma comment(lib, "utilsD")
#else
    #pragma comment(lib, "utils")
#endif



int main(int argc, char *argv[])
{
	CMyZip mz;
	if(argv[1]!=0 && strcmp(argv[1],"zip")==0)
	{
		if(!mz.zip(argv[2], argv[3])) return -1;
	}
	else if(argv[1]!=0 && strcmp(argv[1],"unzip")==0)
	{
		if(!mz.unzip(argv[2], argv[3])) return -1;
	}
	else
	{
		printf("Usage: %s zip [dir] [zipfile]\n", argv[0]);
		printf("       %s unzip [zipfile] [dir]\n", argv[0]);
		return -1;
	}

	return 0;
}

