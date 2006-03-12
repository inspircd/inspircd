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
#include "hashcomp.h"

/* $ModDesc: Povides support for services +r user/chan modes and more */

class ModuleServices : public Module
{
	Server *Srv; 
	bool kludgeme;

 public:
	ModuleServices(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;

		Srv->AddExtendedMode('r',MT_CHANNEL,false,0,0);
		Srv->AddExtendedMode('r',MT_CLIENT,false,0,0);
		Srv->AddExtendedMode('R',MT_CHANNEL,false,0,0);
		Srv->AddExtendedMode('R',MT_CLIENT,false,0,0);
		Srv->AddExtendedMode('M',MT_CHANNEL,false,0,0);

		kludgeme = false;
	}

	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output, "rRM", 4);
	}

	/* <- :stitch.chatspike.net 307 w00t w00t :is a registered nick */
	virtual void OnWhois(userrec* source, userrec* dest)
	{
		if (strchr(dest->modes, 'r'))
		{
			/* user is registered */
			WriteServ(source->fd, "307 %s %s :is a registered nick", source->nick, dest->nick);
		}
	}

	void Implements(char* List)
	{
		List[I_OnWhois] = List[I_OnUserPostNick] = List[I_OnUserPreMessage] = List[I_OnExtendedMode] = List[I_On005Numeric] = List[I_OnUserPreNotice] = List[I_OnUserPreJoin] = 1;
	}

	virtual void OnUserPostNick(userrec* user, const std::string &oldnick)
	{
		/* On nickchange, if they have +r, remove it */
		if (strchr(user->modes,'r'))
		{
			char* modechange[2];
			modechange[0] = user->nick;
			modechange[1] = "-r";
			kludgeme = true;
			Srv->SendMode(modechange,2,user);
			kludgeme = false;
		}
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		
		if (modechar == 'r')
  		{
			if (type == MT_CHANNEL)
			{
				// only a u-lined server may add or remove the +r mode.
				if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)) || (!strcmp(user->server,"") || (strchr(user->nick,'.'))))
				{
					log(DEBUG,"Allowing umode +r, server and nick are: '%s','%s'",user->nick,user->server);
					return 1;
				}
				else
				{
					log(DEBUG,"Only a server can set chanmode +r, server and nick are: '%s','%s'",user->nick,user->server);
					Srv->SendServ(user->fd,"500 "+std::string(user->nick)+" :Only a server may modify the +r channel mode");
				}
			}
			else
			{
				if ((kludgeme) || (Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)) || (!strcmp(user->server,"") || (strchr(user->nick,'.'))))
				{
					log(DEBUG,"Allowing umode +r, server and nick are: '%s','%s'",user->nick,user->server);
					return 1;
				}
				else
				{
					log(DEBUG,"Only a server can set umode +r, server and nick are: '%s','%s'",user->nick,user->server);
					Srv->SendServ(user->fd,"500 "+std::string(user->nick)+" :Only a server may modify the +r user mode");
				}
			}
		}
		else if (modechar == 'R')
		{
			return 1;
		}
		else if (modechar == 'M')
		{
			if (type == MT_CHANNEL)
			{
				return 1;
			}
		}

		return 0;
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			if ((c->IsModeSet('M')) && (!strchr(user->modes,'r')))
			{
				if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)) || (!strcmp(user->server,"")))
				{
					// user is ulined, can speak regardless
					return 0;
				}
				// user messaging a +M channel and is not registered
				Srv->SendServ(user->fd,"477 "+std::string(user->nick)+" "+std::string(c->name)+" :You need a registered nickname to speak on this channel");
				return 1;
			}
		}
		if (target_type == TYPE_USER)
		{
			userrec* u = (userrec*)dest;
			if ((strchr(u->modes,'R')) && (!strchr(user->modes,'r')))
			{
				if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)))
				{
					// user is ulined, can speak regardless
					return 0;
				}
				// user messaging a +R user and is not registered
				Srv->SendServ(user->fd,"477 "+std::string(user->nick)+" "+std::string(u->nick)+" :You need a registered nickname to message this user");
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
				if (!strchr(user->modes,'r'))
				{
					if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)))
					{
						// user is ulined, won't be stopped from joining
						return 0;
					}
					// joining a +R channel and not identified
					Srv->SendServ(user->fd,"477 "+std::string(user->nick)+" "+std::string(chan->name)+" :You need a registered nickname to join this channel");
					return 1;
				}
			}
		}
		return 0;
	}

	virtual ~ModuleServices()
	{
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleServices(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleServicesFactory;
}

