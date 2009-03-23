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

Win32ThreadEngine::Win32ThreadEngine(InspIRCd* Instance) : ThreadEngine(Instance)
{
}

void Win32ThreadEngine::Create(Thread* thread)
{
	Win32ThreadData* data = new Win32ThreadData;
	thread->state = data;

	DWORD ThreadId = 0;
	data->handle = CreateThread(NULL,0,Win32ThreadEngine::Entry,thread,0,&ThreadId);

	if (data->handle == NULL)
	{
		thread->state = NULL;
		delete data;
		throw CoreException(std::string("Unable to create new Win32ThreadEngine: ") + dlerror());
	}
}

Win32ThreadEngine::~Win32ThreadEngine()
{
}

DWORD WINAPI Win32ThreadEngine::Entry(void* parameter)
{
	Thread* pt = reinterpret_cast<Thread*>(parameter);
	pt->Run();
	return 0;
}

void Win32ThreadData::FreeThread(Thread* thread)
{
	thread->SetExitFlag();
	WaitForSingleObject(handle,INFINITE);
}


MutexFactory::MutexFactory(InspIRCd* Instance) : ServerInstance(Instance)
{
}

Mutex* MutexFactory::CreateMutex()
{
	return new Win32Mutex();
}

Win32Mutex::Win32Mutex() : Mutex()
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
