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
	print ("1\n");

	if (!(*MyThread = CreateThread(NULL,0,Win32ThreadEngine::Entry,this,0,&ThreadId)))
	{
		printf ("2\n");
		delete MyThread;
		printf ("3\n");
		Mutex(false);
		printf ("4\n");
		throw CoreException(std::string("Unable to reate new Win32ThreadEngine: ") + dlerror());
	}

	printf ("5\n");
	NewThread = thread_to_init;
	NewThread->Creator = this;
	printf ("6\n");
	NewThread->Extend("winthread", MyThread);
	printf ("7\n");
	Mutex(false);
	printf ("8\n");
}

Win32ThreadEngine::~Win32ThreadEngine()
{
	DeleteCriticalSection(&MyMutex);
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
		WaitForSingleObject(*winthread,INFINITE);
		delete winthread;
	}
}

