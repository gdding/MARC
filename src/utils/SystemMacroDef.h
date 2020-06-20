/*-----------------------------------------------------------------------------
* Copyright (c) 2010 ICT, CAS. All rights reserved.
*   dingguodong@software.ict.ac.cn
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.

* Last-Modified: 2010-02-04
*-----------------------------------------------------------------------------*/
#ifndef _H_SYSTEMMACRODEF_GDDING_INCLUDED
#define _H_SYSTEMMACRODEF_GDDING_INCLUDED

#ifdef WIN32

#pragma comment(lib,"ws2_32.lib")

#include <windows.h>
#include <process.h>
#include <wincon.h> 
#include <nb30.h>
#include <conio.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h>
#include <io.h>

// 锁资源宏定义
#define DEFINE_LOCKER(_locker)					HANDLE _locker
#define INITIALIZE_LOCKER(_locker)				_locker = CreateMutex(NULL, FALSE, NULL)
#define INVALID_LOCKER(_locker)					_locker == NULL
#define LOCK(_locker)							WaitForSingleObject(_locker, INFINITE)
#define UNLOCK(_locker)							ReleaseMutex(_locker)
#define DESTROY_LOCKER(_locker)					CloseHandle(_locker)

// 事件的宏定义
#define DEFINE_EVENT(_event)					HANDLE _event
#define INITIALIZE_EVENT(_event)				_event = CreateEvent(NULL, TRUE, FALSE, NULL)
#define INVALID_EVENT(_event)					_event == NULL
#define BROADCAST_EVENT(_event)					SetEvent(_event);
#define WAIT_EVENT(_event, _locker)				ResetEvent(_event);\
												ReleaseMutex(_locker);\
												WaitForSingleObject(_event, INFINITE);\
												WaitForSingleObject(_locker, INFINITE)
#define DESTROY_EVENT(_event)					CloseHandle(_event)

// 线程相关的宏定义
#define DEFINE_THREAD(_thread)					HANDLE _thread
#define BEGIN_THREAD(_thread, _pfn, _pArg)		_thread = (HANDLE)_beginthread(_pfn, 0, _pArg)    // 返回值0表示错误
#define END_THREAD()							_endthread()
#define FAILED_THREAD(_t)						((_t) <= 0)
#define SUCCEED_THREAD(_t)						((_t) > 0)
#define usleep(_us)								Sleep(_us / 1000)  

// Socket 通信
#define INIT_SOCKET					
#define CLOSE_SOCKET(_socket)					closesocket(_socket)
#define socklen_t								int

// 目录操作
#define FIND_DATA_T								_finddata_t
#define FIND_ISDIR(_data)						(_data.attrib & _A_SUBDIR)
#define FIND_ISFILE(_data)						(!(_data.attrib & _A_SUBDIR))
#define OPEN_DIR(_dir, _path)					long _dir;
#define INVALID_DIR(_dir)						0
#define FIND_FIRST(_dir, _path, _data)			_dir = (long)_findfirst(_path, &_data)
#define FIND_NEXT(_dir, _data)					_findnext(_dir, &_data)
#define VALID_FIND(_f)							((_f) != -1)
#define INVALID_FIND(_f)						((_f) == -1)
#define GET_FILENAME(_data)						_data.name
#define FIND_CLOSE(_dir)						_findclose(_dir)
#define DIR_EXIST(_path)						(_access(_path, 0) != -1)
#define MAKE_DIR(_dir)							_mkdir(_dir)

// 动态链接库加载
#define DEFINE_LIBRARY(_dll)					HINSTANCE _dll
#define LOAD_LIBRARY(_dllfile)					LoadLibrary(_dllfile)
#define GET_FUNC(_dll, _func)					GetProcAddress(_dll, _func)
#define FREE_LIBRARY(_dll)						FreeLibrary(_dll)
#define SystemGetLastError()					WSAGetLastError()
#define SYSTEM_ERROR_EWOULDBLOCK				WSAEWOULDBLOCK 
#define SYSTEM_ERROR_EINVAL						WSAEINVAL
#define SYSTEM_ERROR_NOTINITIALISED				WSANOTINITIALISED
#define SYSTEM_ERROR_ENETDOWN					WSAENETDOWN
#define SYSTEM_ERROR_ENOTSOCK					WSAENOTSOCK
#define SYSTEM_ERROR_EFAULT						WSAEFAULT

#else   // Linux平台

#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h> 
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define _off_t									off_t
#define __int64									long long

// 锁资源宏定义
#define DEFINE_LOCKER(_locker)					pthread_mutex_t _locker
#define INITIALIZE_LOCKER(_locker)				pthread_mutex_init(&_locker, NULL)
#define INVALID_LOCKER(_locker)					0
#define LOCK(_locker)							pthread_mutex_lock(&_locker)
#define UNLOCK(_locker)							pthread_mutex_unlock(&_locker)
#define DESTROY_LOCKER(_locker)					pthread_mutex_destroy(&_locker)

// 事件的宏定义
#define DEFINE_EVENT(_event)					pthread_cond_t _event
#define INITIALIZE_EVENT(_event)				pthread_cond_init(&_event, NULL)
#define INVALID_EVENT(_event)					0
#define BROADCAST_EVENT(_event)					pthread_cond_broadcast(&_event)
#define WAIT_EVENT(_event, _locker)				pthread_cond_wait(&_event, &_locker)
#define DESTROY_EVENT(_event)					pthread_cond_destroy(&_event)

// 线程相关的宏定义
#define DEFINE_THREAD(_thread)					pthread_t _thread
#define BEGIN_THREAD(_thread, _pfn, _pArg)		pthread_create(&_thread, NULL, _pfn, _pArg)
#define END_THREAD()							pthread_detach(pthread_self())
#define FAILED_THREAD(_t)						((_t) != 0)
#define SUCCEED_THREAD(_t)						((_t) == 0)

// Socket通信
#define SOCKET									int
#define INVALID_SOCKET							(~0)
#define SOCKET_ERROR							(-1)
#define CLOSE_SOCKET(_socket)					close(_socket)
#define ioctlsocket(_a, _b, _c)					ioctl(_a, _b, _c)
#define SystemGetLastError()					errno
#define SYSTEM_ERROR_EWOULDBLOCK				EINPROGRESS
#define SYSTEM_ERROR_EINVAL						EINVAL
#define SYSTEM_ERROR_NOTINITIALISED				10093
#define SYSTEM_ERROR_ENETDOWN					ENETDOWN
#define SYSTEM_ERROR_ENOTSOCK					ENOTSOCK
#define SYSTEM_ERROR_EFAULT						EFAULT

// 目录操作
#define FIND_DATA_T								struct dirent*
#define FIND_ISDIR(_data)						(_data->d_type==DT_DIR)
#define FIND_ISFILE(_data)						(_data->d_type==DT_REG)
#define OPEN_DIR(_dir, _path)					DIR* _dir = opendir(_path)
#define INVALID_DIR(_dir)						(_dir == NULL)
#define FIND_FIRST(_dir, _path, _data)			_data = readdir(_dir)
#define FIND_NEXT(_dir, _data)					_data = readdir(_dir)
#define VALID_FIND(_f)							((_f) != NULL)
#define INVALID_FIND(_f)						((_f) == NULL)
#define GET_FILENAME(_data)						_data->d_name
#define FIND_CLOSE(_dir)						closedir(_dir)
#define DIR_EXIST(_path)						(access(_path, 0) != -1)
#define MAKE_DIR(_dir)							mkdir(_dir, 0755)

// 动态链接库加载

#define DEFINE_LIBRARY(_dll)					void* _dll
#define LOAD_LIBRARY(_dllfile)					dlopen(_dllfile, RTLD_LAZY)
#define GET_FUNC(_dll, _func)					dlsym(_dll, _func)
#define FREE_LIBRARY(_dll)						dlclose(_dll)

//Function
#define _strnicmp								strncasecmp
#define _stricmp								strcasecmp
#define _snprintf								snprintf
#define _vsnprintf								vsnprintf
#define stricmp									strcasecmp
#define strnicmp								strncasecmp
#define Sleep(x)								usleep(x*1000)

#endif   // #ifdef _WIN32

#endif //_H_SYSTEMMACRODEF_GDDING_INCLUDED

