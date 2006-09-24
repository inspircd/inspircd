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

#include "inspircd.h"
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

int SelectEngine::DispatchEvents()
{
	int result = 0;
	timeval tval;
	int sresult = 0;
	EventHandler* ev[MAX_DESCRIPTORS];

	FD_ZERO(&wfdset);
	FD_ZERO(&rfdset);

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
			{
				ev[result++] = ref[a->second];
			}
		}
	}

	/** An event handler may remove its own descriptor from the list, therefore it is not
	 * safe to directly iterate over the list and dispatch events there with STL iterators.
	 * Thats a shame because it makes this code slower and more resource intensive, but maybe
	 * the user should stop using select(), as select() smells anyway.
	 */
	for (int i = 0; i < result; i++)
	{
		ServerInstance->Log(DEBUG,"Handle %s event on fd %d",ev[i]->Readable() ? "read" : "write", ev[i]->GetFd());
		ev[i]->HandleEvent(ev[i]->Readable() ? EVENT_READ : EVENT_WRITE);
	}

	return result;
}

std::string SelectEngine::GetName()
{
	return "select";
}
