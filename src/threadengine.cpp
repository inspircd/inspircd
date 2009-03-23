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

/* $Core */

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

Mutex::Mutex()
{
}
