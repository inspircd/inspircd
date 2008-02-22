/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "threadengines/threadengine_win32.h"
#include <pthread.h>

CRITICAL_SECTION MyMutex;

Win32ThreadEngine::Win32ThreadEngine(InspIRCd* Instance) : ThreadEngine(Instance)
{
}

void Win32ThreadEngine::Create(Thread* thread_to_init)
{
	HANDLE* MyThread = new HANDLE;
	DWORD ThreadId = 0;

	if (!(MyThread = CreateThread(NULL,0,Win32ThreadEngine::Entry,this,0,&ThreadId)))
	{
		delete MyThread;
		throw CoreException("Unable to reate new Win32ThreadEngine: " + dlerror());
	}

	NewThread = thread_to_init;
	NewThread->Creator = this;
	NewThread->Extend("winthread", MyThread);
}

Win32ThreadEngine::~Win32ThreadEngine()
{
}

void Win32ThreadEngine::Run()
{
	NewThread->Run();
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
		int rc;
		void* status;
		WaitForSingleObject(*winthread,INFINITE);
		delete winthread;
	}
}

