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

#ifndef __THREADENGINE_PTHREAD__
#define __THREADENGINE_PTHREAD__

#include <pthread.h>
#include "inspircd_config.h"
#include "base.h"
#include "threadengine.h"

class InspIRCd;

class CoreExport PThreadEngine : public ThreadEngine
{
 public:

	PThreadEngine(InspIRCd* Instance);

	virtual ~PThreadEngine();

	void Start(Thread* thread_to_init);

	void FreeThread(Thread* thread);

	const std::string GetName()
	{
		return "posix-thread";
	}
};

class CoreExport ThreadEngineFactory : public classbase
{
 public:
	ThreadEngine* Create(InspIRCd* ServerInstance)
	{
		return new PThreadEngine(ServerInstance);
	}
};

class CoreExport PThreadData : public ThreadData
{
 public:
	pthread_t pthread_id;
	void FreeThread(Thread* toFree);
};

class CoreExport PosixMutex : public Mutex
{
 private:
	pthread_mutex_t putex;
 public:
	PosixMutex();
	virtual void Enable(bool enable);
	~PosixMutex();
};

class CoreExport MutexFactory : public Extensible
{
 protected:
	InspIRCd* ServerInstance;
 public:
	MutexFactory(InspIRCd* Instance);
	Mutex* CreateMutex();
};

#endif
