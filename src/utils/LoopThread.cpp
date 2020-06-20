#include "LoopThread.h"
#include "SystemMacroDef.h"
#include <stdio.h>

void CLoopThread::DefaultRutine(void* param)
{
}

#ifdef WIN32
void CLoopThread::RutineThreadFunc(void *p)
{
	((CLoopThread*)p)->LoopRutine();
}
#else
void* CLoopThread::RutineThreadFunc(void *p)
{
	((CLoopThread*)p)->LoopRutine();
	return NULL;
}
#endif

CLoopThread::CLoopThread()
{
	m_bStop = false;
	m_bStarted = false;
	m_bStopped = true;
	m_pfRutine = DefaultRutine;
	m_nLoopSleep = 10;
	m_pvParam = this;
}

CLoopThread::~CLoopThread()
{
	Stop();
}

void CLoopThread::SetRutine(ThreadRutineFunc pf, void* param)
{
	m_pfRutine = pf;
	m_pvParam = param;
}

bool CLoopThread::Start(int loop_us)
{	
	if(m_bStarted && !m_bStopped) return true;
	m_nLoopSleep = loop_us;
	DEFINE_THREAD(t);
	if (FAILED_THREAD(BEGIN_THREAD(t, RutineThreadFunc, this))) 
	{
		perror("BEGIN_THREAD failed");
		return false;
	}
	while(!m_bStarted) usleep(100000);
	return true;
}

void CLoopThread::Stop()
{
	m_bStop = true;
	while(!m_bStopped){
		usleep(100000);
		m_bStop = true;
	}
	usleep(100000);
	m_bStarted = false;
}

void CLoopThread::LoopRutine()
{
	m_bStopped = false;
	m_bStarted = true;
	while(!m_bStop)
	{
		m_pfRutine(m_pvParam);
		if (m_nLoopSleep > 0) 
			usleep(m_nLoopSleep);
	}
	m_bStopped = true;
    END_THREAD();
}
