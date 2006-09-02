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
#include <map>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

/* $ModDesc: Provides channel mode +f (message flood protection) */



class floodsettings : public classbase
{
 public:
	bool ban;
	int secs;
	int lines;
	time_t reset;
	std::map<userrec*,int> counters;

	floodsettings() : ban(0), secs(0), lines(0) {};
	floodsettings(bool a, int b, int c) : ban(a), secs(b), lines(c)
	{
		reset = time(NULL) + secs;
	};

	void addmessage(userrec* who)
	{
		std::map<userrec*,int>::iterator iter = counters.find(who);
		if (iter != counters.end())
		{
			iter->second++;
		}
		else
		{
			counters[who] = 1;
		}
		if (time(NULL) > reset)
		{
			counters.clear();
			reset = time(NULL) + secs;
		}
	}

	bool shouldkick(userrec* who)
	{
		std::map<userrec*,int>::iterator iter = counters.find(who);
		if (iter != counters.end())
		{
			return (iter->second >= this->lines);
		}
		else return false;
	}

	void clear(userrec* who)
	{
		std::map<userrec*,int>::iterator iter = counters.find(who);
		if (iter != counters.end())
		{
			counters.erase(iter);
		}
	}
};

class MsgFlood : public ModeHandler
{
 public:
	MsgFlood(InspIRCd* Instance) : ModeHandler(Instance, 'f', 1, 0, false, MODETYPE_CHANNEL, false) { }

        ModePair ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
        {
		floodsettings* x;
        	if (channel->GetExt("flood",x))
			return std::make_pair(true, (x->ban ? "*" : "")+ConvToStr(x->lines)+":"+ConvToStr(x->secs));
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
		floodsettings *f;

		if (adding)
		{
			char ndata[MAXBUF];
			char* data = ndata;
			strlcpy(ndata,parameter.c_str(),MAXBUF);
			char* lines = data;
			char* secs = NULL;
			bool ban = false;
			if (*data == '*')
			{
				ban = true;
				lines++;
			}
			else
			{
				ban = false;
			}
			while (*data)
			{
				if (*data == ':')
				{
					*data = 0;
					data++;
					secs = data;
					break;
				}
				else data++;
			}
			if (secs)
			{
				/* Set up the flood parameters for this channel */
				int nlines = atoi(lines);
				int nsecs = atoi(secs);
				if ((nlines<1) || (nsecs<1))
				{
					source->WriteServ("608 %s %s :Invalid flood parameter",source->nick,channel->name);
					parameter = "";
					return MODEACTION_DENY;
				}
				else
				{
					if (!channel->GetExt("flood", f))
					{
						parameter = ConvToStr(nlines) + ":" +ConvToStr(nsecs);
						floodsettings *f = new floodsettings(ban,nsecs,nlines);
						channel->Extend("flood",f);
						channel->SetMode('f', true);
						channel->SetModeParam('f', parameter.c_str(), true);
						return MODEACTION_ALLOW;
					}
				}
			}
			else
			{
				source->WriteServ("608 %s %s :Invalid flood parameter",source->nick,channel->name);
				parameter = "";
				return MODEACTION_DENY;
			}
		}
		else
		{
			if (channel->GetExt("flood", f))
			{
				DELETE(f);
				channel->Shrink("flood");
				channel->SetMode('f', false);
				return MODEACTION_ALLOW;
			}
		}
		
		return MODEACTION_DENY;
	}
};

class ModuleMsgFlood : public Module
{
	
	MsgFlood* mf;
	
 public:
 
	ModuleMsgFlood(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mf = new MsgFlood(ServerInstance);
		ServerInstance->AddMode(mf, 'f');
	}
	
	void ProcessMessages(userrec* user,chanrec* dest, const std::string &text)
	{
		if (IS_LOCAL(user))
		{
			floodsettings *f;
			if (dest->GetExt("flood", f))
			{
				f->addmessage(user);
				if (f->shouldkick(user))
				{
					/* Youre outttta here! */
					f->clear(user);
					if (f->ban)
					{
						const char* parameters[3];
						parameters[0] = dest->name;
						parameters[1] = "+b";
						parameters[2] = user->MakeWildHost();
						ServerInstance->SendMode(parameters,3,user);
						std::deque<std::string> n;
						/* Propogate the ban to other servers.
						 * We dont know what protocol we may be using,
						 * so this event is picked up by our protocol
						 * module and formed into a ban command that
						 * suits the protocol in use.
						 */
						n.push_back(dest->name);
						n.push_back("+b");
						n.push_back(user->MakeWildHost());
						Event rmode((char *)&n, NULL, "send_mode");
						rmode.Send(ServerInstance);
					}
					dest->ServerKickUser(user, "Channel flood triggered (mode +f)", true);
				}
			}
		}
	}

	virtual void OnUserMessage(userrec* user, void* dest, int target_type, const std::string &text, char status)
	{
		if (target_type == TYPE_CHANNEL)
		{
			ProcessMessages(user,(chanrec*)dest,text);
		}
	}

	virtual void OnUserNotice(userrec* user, void* dest, int target_type, const std::string &text, char status)
	{
		if (target_type == TYPE_CHANNEL)
		{
			ProcessMessages(user,(chanrec*)dest,text);
		}
	}

	void OnChannelDelete(chanrec* chan)
	{
		floodsettings* f;
		if (chan->GetExt("flood", f))
		{
			DELETE(f);
			chan->Shrink("flood");
		}
	}

	void Implements(char* List)
	{
		List[I_OnChannelDelete] = List[I_OnUserNotice] = List[I_OnUserMessage] = 1;
	}

	virtual ~ModuleMsgFlood()
	{
		ServerInstance->Modes->DelMode(mf);
		DELETE(mf);
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_COMMON | VF_VENDOR);
	}
};


class ModuleMsgFloodFactory : public ModuleFactory
{
 public:
	ModuleMsgFloodFactory()
	{
	}
	
	~ModuleMsgFloodFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleMsgFlood(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleMsgFloodFactory;
}

