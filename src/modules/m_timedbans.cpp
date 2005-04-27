/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Adds timed bans */

/*
 * ToDo:
 *   Err... not a lot really.
 */ 

#include <stdio.h>
#include <vector>
#include "users.h"
#include "channels.h"
#include "modules.h"

Server *Srv;
	 
class TimedBan
{
 public:
	std::string channel;
	std::string mask;
	std::string expire;
};

typedef std::vector<TimedBan> timedbans;
timedbans TimedBanList;

void handle_tban(char **parameters, int pcnt, userrec *user)
{
	chanrec* channel = Srv->FindChannel(parameters[0]);
	if (channel)
	{
		std::string cm = Srv->ChanMode(user,channel);
		if ((cm == "%") || (cm == "@"))
		{
			TimedBan T;
			std::string channelname = parameters[0];
			unsigned long expire = Srv->Duration(parameters[1]) + time(NULL);
			char duration[MAXBUF];
			snprintf(duration,MAXBUF,"%lu",Srv->Duration(parameters[1]));
			std::string mask = parameters[2];
			char *setban[3];
			setban[0] = parameters[0];
			setban[1] = "+b";
			setban[2] = parameters[2];
			// use CallCommandHandler to make it so that the user sets the mode
			// themselves
			Srv->CallCommandHandler("MODE",setban,3,user);
			T.channel = channelname;
			T.mask = mask;
			T.expire = expire;
			TimedBanList.push_back(T);
			Srv->SendChannelServerNotice(Srv->GetServerName(),channel,std::string(user->nick)+" added a timed ban on "+mask+" lasting for "+std::string(duration)+" seconds.");
		}
		else WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, channel->name);
	}
	WriteServ(user->fd,"401 %s %s :No such channel",user->nick, parameters[0]);
}

class ModuleTimedBans : public Module
{
 public:
	ModuleTimedBans()
	{
		Srv = new Server;
		Srv->AddCommand("TBAN",handle_tban,0,3,"m_timedbans.so");
	}
	
	virtual ~ModuleTimedBans()
	{
		delete Srv;
	}

	virtual void OnBackgroundTimer(time_t curtime)
	{
		bool again = true;
		while (again)
		{
			again = false;
			for (timedbans::iterator i = TimedBanList.begin(); i < TimedBanList.end(); i++)
			{
				if (i->expire >= curtime)
				{
					chanrec* cr = Srv->FindChannel(i->channel);
					again = true;
					if (cr)
					{
						char *setban[3];
						setban[0] = i->channel.c_str();
						setban[1] = "-b";
						setban[2] = i->mask.c_str();
						Srv->SendMode(setban,3,NULL);
						Srv->SendChannelServerNotice(Srv->GetServerName(),channel,"Timed ban on "+i->mask+" expired.");
					}
					TimedBanList.erase(i);
					break;
				}
			}
		}
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR);
	}
};


class ModuleTimedBansFactory : public ModuleFactory
{
 public:
	ModuleTimedBansFactory()
	{
	}
	
	~ModuleTimedBansFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleTimedBans;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleTimedBansFactory;
}
