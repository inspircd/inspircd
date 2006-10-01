/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
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

using namespace std;

/* $ModDesc: Adds timed bans */

#include <stdio.h>
#include <vector>
#include "users.h"
#include "channels.h"
#include "modules.h"

#include "hashcomp.h"
#include "configreader.h"
#include "inspircd.h"




class TimedBan : public classbase
{
 public:
	std::string channel;
	std::string mask;
	time_t expire;
};

typedef std::vector<TimedBan> timedbans;
timedbans TimedBanList;

class cmd_tban : public command_t
{
 public:
 cmd_tban (InspIRCd* Instance) : command_t(Instance,"TBAN", 0, 3)
	{
		this->source = "m_timedbans.so";
		syntax = "<channel> <duration> <banmask>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		chanrec* channel = ServerInstance->FindChan(parameters[0]);
		if (channel)
		{
			int cm = channel->GetStatus(user);
			if ((cm == STATUS_HOP) || (cm == STATUS_OP))
			{
				if (!ServerInstance->IsValidMask(parameters[2]))
				{
					user->WriteServ("NOTICE "+std::string(user->nick)+" :Invalid ban mask");
					return CMD_FAILURE;
				}
				for (BanList::iterator i = channel->bans.begin(); i != channel->bans.end(); i++)
				{
					if (!strcasecmp(i->data,parameters[2]))
					{
						user->WriteServ("NOTICE "+std::string(user->nick)+" :The ban "+std::string(parameters[2])+" is already on the banlist of "+std::string(parameters[0]));
						return CMD_FAILURE;
					}
				}
				TimedBan T;
				std::string channelname = parameters[0];
				unsigned long expire = ServerInstance->Duration(parameters[1]) + time(NULL);
				if (ServerInstance->Duration(parameters[1]) < 1)
				{
					user->WriteServ("NOTICE "+std::string(user->nick)+" :Invalid ban time");
					return CMD_FAILURE;
				}
				char duration[MAXBUF];
				snprintf(duration,MAXBUF,"%lu",ServerInstance->Duration(parameters[1]));
				std::string mask = parameters[2];
				const char *setban[32];
				setban[0] = parameters[0];
				setban[1] = "+b";
				setban[2] = parameters[2];
				// use CallCommandHandler to make it so that the user sets the mode
				// themselves
				ServerInstance->CallCommandHandler("MODE",setban,3,user);
				/* Check if the ban was actually added (e.g. banlist was NOT full) */
				bool was_added = false;
				for (BanList::iterator i = channel->bans.begin(); i != channel->bans.end(); i++)
					if (!strcasecmp(i->data,mask.c_str()))
						was_added = true;
				if (was_added)
				{
					T.channel = channelname;
					T.mask = mask;
					T.expire = expire;
					TimedBanList.push_back(T);
					channel->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s added a timed ban on %s lasting for %s seconds.", channel->name, user->nick, mask.c_str(), duration);
					return CMD_SUCCESS;
				}
				return CMD_FAILURE;
			}
			else user->WriteServ("482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, channel->name);
			return CMD_FAILURE;
		}
		user->WriteServ("401 %s %s :No such channel",user->nick, parameters[0]);
		return CMD_FAILURE;
	}
};

class ModuleTimedBans : public Module
{
	cmd_tban* mycommand;
 public:
	ModuleTimedBans(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mycommand = new cmd_tban(ServerInstance);
		ServerInstance->AddCommand(mycommand);
		TimedBanList.clear();
	}
	
	virtual ~ModuleTimedBans()
	{
		TimedBanList.clear();
	}

	void Implements(char* List)
	{
		List[I_OnDelBan] = List[I_OnBackgroundTimer] = 1;
	}

	virtual int OnDelBan(userrec* source, chanrec* chan, const std::string &banmask)
	{
		irc::string listitem = banmask.c_str();
		irc::string thischan = chan->name;
		for (timedbans::iterator i = TimedBanList.begin(); i < TimedBanList.end(); i++)
		{
			irc::string target = i->mask.c_str();
			irc::string tchan = i->channel.c_str();
			if ((listitem == target) && (tchan == thischan))
			{
				TimedBanList.erase(i);
				break;
			}
		}
		return 0;
	}

	virtual void OnBackgroundTimer(time_t curtime)
	{
		bool again = true;
		while (again)
		{
			again = false;
			for (timedbans::iterator i = TimedBanList.begin(); i < TimedBanList.end(); i++)
			{
				if (curtime > i->expire)
				{
					chanrec* cr = ServerInstance->FindChan(i->channel);
					again = true;
					if (cr)
					{
						cr->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :Timed ban on %s expired.", cr->name, i->mask.c_str());
						const char *setban[3];
						setban[0] = i->channel.c_str();
						setban[1] = "-b";
						setban[2] = i->mask.c_str();
						// kludge alert!
						// ::SendMode expects a userrec* to send the numeric replies
						// back to, so we create it a fake user that isnt in the user
						// hash and set its descriptor to FD_MAGIC_NUMBER so the data
						// falls into the abyss :p
						userrec* temp = new userrec(ServerInstance);
						temp->SetFd(FD_MAGIC_NUMBER);
                                                /* FIX: Send mode remotely*/
                                                std::deque<std::string> n;
                                                n.push_back(setban[0]);
                                                n.push_back("-b");
                                                n.push_back(setban[2]);
						ServerInstance->SendMode(setban,3,temp);
                                                Event rmode((char *)&n, NULL, "send_mode");
                                                rmode.Send(ServerInstance);
						DELETE(temp);
					}
					else
					{
						/* Where the hell did our channel go?! */
						TimedBanList.erase(i);
					}
					// we used to delete the item here, but we dont need to as the servermode above does it for us,
					break;
				}
			}
		}
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR,API_VERSION);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleTimedBans(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleTimedBansFactory;
}
