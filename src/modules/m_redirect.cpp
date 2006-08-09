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

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "inspircd.h"

/* $ModDesc: Provides channel mode +L (limit redirection) */

extern InspIRCd* ServerInstance;

class Redirect : public ModeHandler
{
	Server* Srv;
 public:
	Redirect(Server* s) : ModeHandler('L', 1, 0, false, MODETYPE_CHANNEL, false), Srv(s) { }

        ModePair ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
        {
                if (channel->IsModeSet('L'))
                        return std::make_pair(true, channel->GetModeParameter('L'));
                else
                        return std::make_pair(false, parameter);
        }

        bool CheckTimeStamp(time_t theirs, time_t ours, const std::string &their_param, const std::string &our_param, chanrec* channel)
	{
		/* When TS is equal, the alphabetically later one wins */
		return (their_param < our_param);
        }
	
	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			chanrec* c = NULL;

			if (!IsValidChannelName(parameter.c_str()))
			{
				source->WriteServ("403 %s %s :Invalid channel name",source->nick, parameter.c_str());
				parameter = "";
				return MODEACTION_DENY;
			}

			c = Srv->FindChannel(parameter);
			if (c)
			{
				/* Fix by brain: Dont let a channel be linked to *itself* either */
				if (IS_LOCAL(source))
				{
					if ((c == channel) || (c->IsModeSet('L')))
					{
						source->WriteServ("690 %s :Circular or chained +L to %s not allowed (Channel already has +L). Pack of wild dogs has been unleashed.",source->nick,parameter.c_str());
						parameter = "";
						return MODEACTION_DENY;
					}
					else
					{
						for (chan_hash::const_iterator i = ServerInstance->chanlist.begin(); i != ServerInstance->chanlist.end(); i++)
						{
							if ((i->second != channel) && (i->second->IsModeSet('L')) && (irc::string(i->second->GetModeParameter('L').c_str()) == irc::string(channel->name)))
							{
								source->WriteServ("690 %s :Circular or chained +L to %s not allowed (Already forwarded here from %s). Angry monkeys dispatched.",source->nick,parameter.c_str(),i->second->name);
								return MODEACTION_DENY;
							}
						}
					}
				}
			}

			channel->SetMode('L', true);
			channel->SetModeParam('L', parameter.c_str(), true);
			return MODEACTION_ALLOW;
		}
		else
		{
			if (channel->IsModeSet('L'))
			{
				channel->SetMode('L', false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
		
	}
};

class ModuleRedirect : public Module
{
	Server *Srv;
	Redirect* re;
	
 public:
 
	ModuleRedirect(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		re = new Redirect(Me);
		Srv->AddMode(re, 'L');
	}
	
	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnUserPreJoin] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output, "L", 3);
	}
	
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		if (chan)
		{
			if (chan->IsModeSet('L'))
			{
				if (chan->GetUserCounter() >= chan->limit)
				{
					std::string channel = chan->GetModeParameter('L');
					user->WriteServ("470 %s :%s has become full, so you are automatically being transferred to the linked channel %s",user->nick,cname,channel.c_str());
					chanrec::JoinUser(ServerInstance, user, channel.c_str(), false);
					return 1;
				}
			}
		}
		return 0;
	}

	virtual ~ModuleRedirect()
	{
		DELETE(re);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleRedirectFactory : public ModuleFactory
{
 public:
	ModuleRedirectFactory()
	{
	}
	
	~ModuleRedirectFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleRedirect(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleRedirectFactory;
}

