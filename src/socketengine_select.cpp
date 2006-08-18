/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd_config.h"
#include "globals.h"
#include "inspircd.h"
#include <vector>
#include <string>
#include <sys/select.h>
#include "socketengine_select.h"


SelectEngine::SelectEngine(InspIRCd* Instance) : SocketEngine(Instance)
{
	ServerInstance->Log(DEBUG,"SelectEngine::SelectEngine()");
	EngineHandle = 0;
	CurrentSetSize = 0;
}

SelectEngine::~SelectEngine()
{
	ServerInstance->Log(DEBUG,"SelectEngine::~SelectEngine()");
}

bool SelectEngine::AddFd(EventHandler* eh)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
	{
		ServerInstance->Log(DEFAULT,"ERROR: FD of %d added above max of %d",fd,MAX_DESCRIPTORS);
		return false;
	}
	if (GetRemainingFds() <= 1)
	{
		ServerInstance->Log(DEFAULT,"ERROR: System out of file descriptors!");
		return false;
	}

	fds[fd] = fd;

	if (ref[fd])
		return false;

	ref[fd] = eh;
	ServerInstance->Log(DEBUG,"Add socket %d",fd);

	CurrentSetSize++;
	return true;
}

bool SelectEngine::DelFd(EventHandler* eh)
{
	int fd = eh->GetFd();

	ServerInstance->Log(DEBUG,"SelectEngine::DelFd(%d)",fd);

	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;

	std::map<int,int>::iterator t = fds.find(fd);
	if (t != fds.end())
	{
		fds.erase(t);
		ServerInstance->Log(DEBUG,"Deleted fd %d",fd);
	}

	CurrentSetSize--;
	ref[fd] = NULL;
	return true;
}

int SelectEngine::GetMaxFds()
{
	return FD_SETSIZE;
}

int SelectEngine::GetRemainingFds()
{
	return FD_SETSIZE - CurrentSetSize;
}

int SelectEngine::Wait(EventHandler** fdlist)
{
	int result = 0;

	FD_ZERO(&wfdset);
	FD_ZERO(&rfdset);
	timeval tval;
	int sresult;
	for (std::map<int,int>::iterator a = fds.begin(); a != fds.end(); a++)
	{
		if (ref[a->second]->Readable())
		{
			FD_SET (a->second, &rfdset);
		}
		else
		{
			FD_SET (a->second, &wfdset);
		}
		
	}
	tval.tv_sec = 0;
	tval.tv_usec = 50L;
	sresult = select(FD_SETSIZE, &rfdset, &wfdset, NULL, &tval);
	if (sresult > 0)
	{
		for (std::map<int,int>::iterator a = fds.begin(); a != fds.end(); a++)
		{
			if ((FD_ISSET (a->second, &rfdset)) || (FD_ISSET (a->second, &wfdset)))
				fdlist[result++] = ref[a->second];
		}
	}

	return result;
}

std::string SelectEngine::GetName()
{
	return "select";
}
