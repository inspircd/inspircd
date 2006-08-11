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
#include <string>
#include "helperfuncs.h"
#include "commands.h"
#include "hashcomp.h"
#include "inspircd.h"

static bool kludgeme = false;

/* $ModDesc: Povides support for services +r user/chan modes and more */



class Channel_r : public ModeHandler
{
	
 public:
	Channel_r(InspIRCd* Instance) : ModeHandler(Instance, 'r', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		// only a u-lined server may add or remove the +r mode.
		if ((is_uline(source->nick)) || (is_uline(source->server)) || (!*source->server || (strchr(source->nick,'.'))))
		{
			log(DEBUG,"Allowing cmode +r, server and nick are: '%s','%s'",source->nick,source->server);
			channel->SetMode('r',adding);
			return MODEACTION_ALLOW;
		}
		else
		{
			log(DEBUG,"Only a server can set chanmode +r, server and nick are: '%s','%s'",source->nick,source->server);
			source->WriteServ("500 "+std::string(source->nick)+" :Only a server may modify the +r channel mode");
			return MODEACTION_DENY;
		}
	}
};

class User_r : public ModeHandler
{
	
 public:
	User_r(InspIRCd* Instance) : ModeHandler(Instance, 'r', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if ((kludgeme) || (is_uline(source->nick)) || (is_uline(source->server)) || (!*source->server || (strchr(source->nick,'.'))))
		{
			log(DEBUG,"Allowing umode +r, server and nick are: '%s','%s'",source->nick,source->server);
			dest->SetMode('r',adding);
			return MODEACTION_ALLOW;
		}
		else
		{
			log(DEBUG,"Only a server can set umode +r, server and nick are: '%s','%s'",source->nick, source->server);
			source->WriteServ("500 "+std::string(source->nick)+" :Only a server may modify the +r user mode");
			return MODEACTION_DENY;
		}
	}
};

class Channel_R : public ModeHandler
{
 public:
	Channel_R(InspIRCd* Instance) : ModeHandler(Instance, 'R', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('R'))
			{
				channel->SetMode('R',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('R'))
			{
				channel->SetMode('R',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class User_R : public ModeHandler
{
 public:
	User_R(InspIRCd* Instance) : ModeHandler(Instance, 'R', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('R'))
			{
				dest->SetMode('R',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('R'))
			{
				dest->SetMode('R',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class Channel_M : public ModeHandler
{
 public:
	Channel_M(InspIRCd* Instance) : ModeHandler(Instance, 'M', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('M'))
			{
				channel->SetMode('M',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('M'))
			{
				channel->SetMode('M',true);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleServices : public Module
{
	
	Channel_r* m1;
	Channel_R* m2;
	Channel_M* m3;
	User_r* m4;
	User_R* m5;
 public:
	ModuleServices(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		m1 = new Channel_r(ServerInstance);
		m2 = new Channel_R(ServerInstance);
		m3 = new Channel_M(ServerInstance);
		m4 = new User_r(ServerInstance);
		m5 = new User_R(ServerInstance);
		ServerInstance->AddMode(m1, 'r');
		ServerInstance->AddMode(m2, 'R');
		ServerInstance->AddMode(m3, 'M');
		ServerInstance->AddMode(m4, 'r');
		ServerInstance->AddMode(m5, 'R');
		kludgeme = false;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->ModeGrok->InsertMode(output, "rRM", 4);
	}

	/* <- :stitch.chatspike.net 307 w00t w00t :is a registered nick */
	virtual void OnWhois(userrec* source, userrec* dest)
	{
		if (dest->IsModeSet('r'))
		{
			/* user is registered */
			source->WriteServ("307 %s %s :is a registered nick", source->nick, dest->nick);
		}
	}

	void Implements(char* List)
	{
		List[I_OnWhois] = List[I_OnUserPostNick] = List[I_OnUserPreMessage] = List[I_On005Numeric] = List[I_OnUserPreNotice] = List[I_OnUserPreJoin] = 1;
	}

	virtual void OnUserPostNick(userrec* user, const std::string &oldnick)
	{
		/* On nickchange, if they have +r, remove it */
		if (user->IsModeSet('r'))
		{
			const char* modechange[2];
			modechange[0] = user->nick;
			modechange[1] = "-r";
			kludgeme = true;
			ServerInstance->SendMode(modechange,2,user);
			kludgeme = false;
		}
	}
	
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			if ((c->IsModeSet('M')) && (!user->IsModeSet('r')))
			{
				if ((is_uline(user->nick)) || (is_uline(user->server)) || (!strcmp(user->server,"")))
				{
					// user is ulined, can speak regardless
					return 0;
				}
				// user messaging a +M channel and is not registered
				user->WriteServ("477 "+std::string(user->nick)+" "+std::string(c->name)+" :You need a registered nickname to speak on this channel");
				return 1;
			}
		}
		if (target_type == TYPE_USER)
		{
			userrec* u = (userrec*)dest;
			if ((u->IsModeSet('R')) && (user->IsModeSet('r')))
			{
				if ((is_uline(user->nick)) || (is_uline(user->server)))
				{
					// user is ulined, can speak regardless
					return 0;
				}
				// user messaging a +R user and is not registered
				user->WriteServ("477 "+std::string(user->nick)+" "+std::string(u->nick)+" :You need a registered nickname to message this user");
				return 1;
			}
		}
		return 0;
	}
 	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return OnUserPreMessage(user,dest,target_type,text,status);
	}
 	
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		if (chan)
		{
			if (chan->IsModeSet('R'))
			{
				if (user->IsModeSet('r'))
				{
					if ((is_uline(user->nick)) || (is_uline(user->server)))
					{
						// user is ulined, won't be stopped from joining
						return 0;
					}
					// joining a +R channel and not identified
					user->WriteServ("477 "+std::string(user->nick)+" "+std::string(chan->name)+" :You need a registered nickname to join this channel");
					return 1;
				}
			}
		}
		return 0;
	}

	virtual ~ModuleServices()
	{
		DELETE(m1);
		DELETE(m2);
		DELETE(m3);
		DELETE(m4);
		DELETE(m5);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleServicesFactory : public ModuleFactory
{
 public:
	ModuleServicesFactory()
	{
	}
	
	~ModuleServicesFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleServices(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleServicesFactory;
}

