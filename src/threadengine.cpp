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

/* $Core: libIRCDthreadengine */

/*********        DEFAULTS       **********/
/* $ExtraSources: threadengines/threadengine_pthread.cpp */
/* $ExtraObjects: threadengine_pthread.o */

#include "inspircd.h"
#include "threadengine.h"

ThreadEngine::ThreadEngine(InspIRCd* Instance) : ServerInstance(Instance)
{
}

ThreadEngine::~ThreadEngine()
{
}

