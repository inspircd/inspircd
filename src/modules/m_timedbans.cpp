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
#include "helperfuncs.h"
#include "hashcomp.h"

static Server *Srv;
	 
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
	cmd_tban () : command_t("TBAN", 0, 3)
	{
		this->source = "m_timedbans.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		chanrec* channel = Srv->FindChannel(parameters[0]);
		if (channel)
		{
			std::string cm = Srv->ChanMode(user,channel);
			if ((cm == "%") || (cm == "@"))
			{
				if (!Srv->IsValidMask(parameters[2]))
				{
					Srv->SendServ(user->fd,"NOTICE "+std::string(user->nick)+" :Invalid ban mask");
					return;
				}
				for (timedbans::iterator i = TimedBanList.begin(); i < TimedBanList.end(); i++)
				{
					irc::string listitem = i->mask.c_str();
					irc::string target = parameters[2];
					irc::string listchan = i->channel.c_str();
					irc::string targetchan = parameters[0];
					if ((listitem == target) && (listchan == targetchan))
					{
						Srv->SendServ(user->fd,"NOTICE "+std::string(user->nick)+" :The ban "+std::string(parameters[2])+" is already on the banlist of "+std::string(parameters[0]));
						return;
					}
				}
				TimedBan T;
				std::string channelname = parameters[0];
				unsigned long expire = Srv->CalcDuration(parameters[1]) + time(NULL);
				if (Srv->CalcDuration(parameters[1]) < 1)
				{
					Srv->SendServ(user->fd,"NOTICE "+std::string(user->nick)+" :Invalid ban time");
					return;
				}
				char duration[MAXBUF];
				snprintf(duration,MAXBUF,"%lu",Srv->CalcDuration(parameters[1]));
				std::string mask = parameters[2];
				char *setban[32];
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
				Srv->SendChannelServerNotice(Srv->GetServerName(),channel,"NOTICE "+std::string(channel->name)+" :"+std::string(user->nick)+" added a timed ban on "+mask+" lasting for "+std::string(duration)+" seconds.");
				return;
			}
			else WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, channel->name);
			return;
		}
		WriteServ(user->fd,"401 %s %s :No such channel",user->nick, parameters[0]);
	}
};

class ModuleTimedBans : public Module
{
	cmd_tban* mycommand;
 public:
	ModuleTimedBans(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_tban();
		Srv->AddCommand(mycommand);
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
		for (timedbans::iterator i = TimedBanList.begin(); i < TimedBanList.end(); i++)
		{
			irc::string listitem = banmask.c_str();
			irc::string target = i->mask.c_str();
			if (listitem == target)
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
					chanrec* cr = Srv->FindChannel(i->channel);
					again = true;
					if (cr)
					{
						Srv->SendChannelServerNotice(Srv->GetServerName(),cr,"NOTICE "+std::string(cr->name)+" :Timed ban on "+i->mask+" expired.");
						char *setban[3];
						setban[0] = (char*)i->channel.c_str();
						setban[1] = "-b";
						setban[2] = (char*)i->mask.c_str();
						// kludge alert!
						// ::SendMode expects a userrec* to send the numeric replies
						// back to, so we create it a fake user that isnt in the user
						// hash and set its descriptor to FD_MAGIC_NUMBER so the data
						// falls into the abyss :p
						userrec* temp = new userrec;
						temp->fd = FD_MAGIC_NUMBER;
						temp->server = "";
						Srv->SendMode(setban,3,temp);
						DELETE(temp);
					}
					// we used to delete the item here, but we dont need to as the servermode above does it for us,
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleTimedBans(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleTimedBansFactory;
}
