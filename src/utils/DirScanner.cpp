#include "DirScanner.h"
#include "Utility.h"

CDirScanner::CDirScanner(const char* dir, bool bRecursive)
{
	assert(dir != NULL && dir[0] != 0);
	m_sBaseDir = dir;
	NormalizePath(m_sBaseDir);
	m_bRecursive = bRecursive;
	perform(m_sBaseDir.c_str(), bRecursive);
}

void CDirScanner::perform(const char *dir, bool bRecursive)
{
	if(dir == NULL || dir[0]==0) return;
	char chFilePath[1024] = {0};
	strcpy(chFilePath, dir);
	NormalizePath(chFilePath);

	char _path[1024] = {0};
#ifdef _WIN32
	sprintf(_path, "%s*.*", chFilePath);
#else
	sprintf(_path, "%s", chFilePath);
#endif

	FIND_DATA_T _data;
	OPEN_DIR(_dir, _path);
	if(INVALID_DIR(_dir)) return ;
	if(VALID_FIND(FIND_FIRST(_dir, _path, _data)))
	{
		do
		{
			if (FIND_ISFILE(_data))
			{
				string sFile(chFilePath);
				sFile += GET_FILENAME(_data);
				OnFileFound(sFile.c_str());
			}
			else if(FIND_ISDIR(_data))
			{
				const char* sDirName = GET_FILENAME(_data);
				if(strcmp(sDirName, ".") != 0 && strcmp(sDirName, "..") != 0)
				{
					string sSubDir(chFilePath);
					sSubDir += GET_FILENAME(_data);
#ifdef _WIN32
					sSubDir += "\\";
#else
					sSubDir += "/";
#endif
					if(bRecursive)
						perform(sSubDir.c_str(), true);
					OnSubDirFound(sSubDir.c_str(), chFilePath);
				}
			}
		}while(VALID_FIND(FIND_NEXT(_dir, _data)));
	}
	FIND_CLOSE(_dir);
}

void CDirScanner::OnFileFound(const char *filename)
{
	string sFileName = filename;
	m_vFileFullList.push_back(sFileName);

	string sNewFileName = "";
	string::size_type pos = sFileName.find(m_sBaseDir);
	if(pos == 0)
	{
		sFileName.replace(pos, m_sBaseDir.length(), sNewFileName);
	}
	m_vFileList.push_back(sFileName);
	m_vAllList.push_back(sFileName);
}

void CDirScanner::OnSubDirFound(const char *currentdir, const char *parentdir)
{
	string sDirName = currentdir;

	string sNewDirName = "";
	string::size_type pos = sDirName.find(m_sBaseDir);
	if(pos == 0)
	{
		sDirName.replace(pos, m_sBaseDir.length(), sNewDirName);
	}
	m_vDirList.push_back(sDirName);
	m_vAllList.push_back(sDirName);
}
