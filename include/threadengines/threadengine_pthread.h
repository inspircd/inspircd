/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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

	bool Mutex(bool enable);

	void Run();

	static void* Entry(void* parameter);

	void Create(Thread* thread_to_init);

	void FreeThread(Thread* thread);
};

#endif
