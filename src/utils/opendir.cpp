#include  "opendir.h" 

#ifdef _WIN32
#include <errno.h>

DIR* opendir(const char *dir)
{
	DIR *dp;  
	char filespec[_MAX_FNAME];  
	intptr_t handle;  
	int index;  
	strncpy(filespec,dir,_MAX_FNAME);
	index = (int)strlen(filespec) - 1;
	if(index>=0 && (filespec[index]=='/' || filespec[index] == '\\'))  
		filespec[index]  =  '\0';  
	strcat(filespec,  "\\*");  
	dp = (DIR  *)malloc(sizeof(DIR));  
	dp->offset = 0;  
	dp->finished = 0;  
	dp->dir = strdup(dir);  
	if((handle = _findfirst(filespec,  &(dp->fileinfo))) < 0)
	{
		if(errno  ==  ENOENT)
			dp->finished = 1;  
		else
			return NULL;  
	}  
	dp->handle = handle;  
	return  dp;  
}  

struct dirent* readdir(DIR  *dp)
{  
	if(!dp || dp->finished)
		return  NULL;
	if(dp->offset!=0)   
	{
		if(_findnext(dp->handle,&(dp->fileinfo)) < 0)
		{  
			dp->finished = 1;
			return NULL;
		}
	}
	dp->offset++;
	strncpy(dp->dent.d_name, dp->fileinfo.name, _MAX_FNAME);
	dp->dent.d_ino = 1;
	dp->dent.d_reclen = (int)strlen(dp->dent.d_name);
	dp->dent.d_off = dp->offset;
	return &(dp->dent);
}  

int closedir(DIR *dp)  
{  
	if(!dp) return  0;

	_findclose(dp->handle);
	if(dp->dir)
	{
		free(dp->dir);
		dp->dir = NULL;
	}
	if(dp)
	{
		free(dp);
		dp = NULL;
	}
	return 0;
}

#endif 
