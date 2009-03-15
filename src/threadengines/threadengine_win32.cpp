/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "threadengines/threadengine_win32.h"

CRITICAL_SECTION MyMutex;

Win32ThreadEngine::Win32ThreadEngine(InspIRCd* Instance) : ThreadEngine(Instance)
{
	InitializeCriticalSection(&MyMutex);
}

void Win32ThreadEngine::Create(Thread* thread_to_init)
{
	Mutex(true);
	HANDLE* MyThread = new HANDLE;
	DWORD ThreadId = 0;

	if (NULL == (*MyThread = CreateThread(NULL,0,Win32ThreadEngine::Entry,this,0,&ThreadId)))
	{
		delete MyThread;
		Mutex(false);
		throw CoreException(std::string("Unable to create new Win32ThreadEngine: ") + dlerror());
	}

	NewThread = thread_to_init;
	NewThread->Creator = this;
	NewThread->Extend("winthread", MyThread);
	Mutex(false);

	while (NewThread)
		SleepEx(100, false);
}

Win32ThreadEngine::~Win32ThreadEngine()
{
	DeleteCriticalSection(&MyMutex);
}

void Win32ThreadEngine::Run()
{
	Mutex(true);
	Thread* nt = NewThread;
	NewThread = NULL;
	Mutex(false);
	nt->Run();
}

bool Win32ThreadEngine::Mutex(bool enable)
{
	if (enable)
		EnterCriticalSection(&MyMutex);
	else
		LeaveCriticalSection(&MyMutex);

	return false;
}

DWORD WINAPI Win32ThreadEngine::Entry(void* parameter)
{
	ThreadEngine * pt = (ThreadEngine*)parameter;
	pt->Run();
	return 0;
}

void Win32ThreadEngine::FreeThread(Thread* thread)
{
	HANDLE* winthread = NULL;
	if (thread->GetExt("winthread", winthread))
	{
		thread->SetExitFlag();
		WaitForSingleObject(*winthread,INFINITE);
		delete winthread;
	}
}


MutexFactory::MutexFactory(InspIRCd* Instance) : ServerInstance(Instance)
{
}

Mutex* MutexFactory::CreateMutex()
{
	return new Win32Mutex(this->ServerInstance);
}

Win32Mutex::Win32Mutex(InspIRCd* Instance) : Mutex(Instance)
{
	InitializeCriticalSection(&wutex);
}

Win32Mutex::~Win32Mutex()
{
	DeleteCriticalSection(&wutex);
}

void Win32Mutex::Enable(bool enable)
{
	if (enable)
		EnterCriticalSection(&wutex);
	else
		LeaveCriticalSection(&wutex);
}