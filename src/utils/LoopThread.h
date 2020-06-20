/*-----------------------------------------------------------------------------
* Copyright (c) 2007~2010 ICT, CAS. All rights reserved.
*   dingguodong@software.ict.ac.cn
*
* This file is the confidential and proprietary property of 
* ICT, CAS and the posession or use of this file requires 
* a written license from the author.

* Last-Modified: 2010-02-04
*-----------------------------------------------------------------------------*/
#ifndef _H_LOOPTHREAD_GDDING_INCLUDED_20071222
#define _H_LOOPTHREAD_GDDING_INCLUDED_20071222

typedef void (*ThreadRutineFunc)(void* param);
class CLoopThread
{
public:
	CLoopThread();
	virtual ~CLoopThread();

public:
	void SetRutine(ThreadRutineFunc pf, void* param);
	bool Start(int loop_us=10);
	void Stop();

private:
	ThreadRutineFunc m_pfRutine;
	void* m_pvParam;
	static void DefaultRutine(void* param);

#ifdef WIN32
	static void RutineThreadFunc(void *p);
#else
	static void* RutineThreadFunc(void *p);
#endif
	void LoopRutine();
	int	 m_nLoopSleep;	//ÿ��ѭ�����΢����
	bool m_bStop;		//�Ƿ�ֹͣ�߳�
	bool m_bStarted;	//�߳��Ƿ��Ѿ�����
	bool m_bStopped;	//�߳��Ƿ��Ѿ���ֹ
};

#endif //_H_LOOPTHREAD_GDDING_INCLUDED_20071222
