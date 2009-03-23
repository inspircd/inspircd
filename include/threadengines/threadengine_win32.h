/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __THREADENGINE_WIN32THREAD__
#define __THREADENGINE_WIN32THREAD__

#include "inspircd_config.h"
#include "base.h"
#include "threadengine.h"

class InspIRCd;

class CoreExport Win32ThreadEngine : public ThreadEngine
{
 public:

	Win32ThreadEngine(InspIRCd* Instance);

	virtual ~Win32ThreadEngine();

	static DWORD WINAPI Entry(void* parameter);

	void Start(Thread* thread_to_init);

	const std::string GetName()
	{
		return "windows-thread";
	}
};

class CoreExport ThreadEngineFactory : public classbase
{
 public:
	ThreadEngine* Create(InspIRCd* ServerInstance)
	{
		return new Win32ThreadEngine(ServerInstance);
	}
};

class CoreExport Win32ThreadData : public ThreadData
{
 public:
	HANDLE handle;
	void FreeThread(Thread* toFree);
};

class CoreExport Win32Mutex : public Mutex
{
 private:
	CRITICAL_SECTION wutex;
 public:
	Win32Mutex();
	virtual void Enable(bool enable);
	~Win32Mutex();
};

class CoreExport MutexFactory : public Extensible
{
 protected:
	InspIRCd* ServerInstance;
 public:
	MutexFactory(InspIRCd* Instance);
	virtual Mutex* CreateMutex();
};

#endif

