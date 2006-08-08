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

/* $ModDesc: Provides the UNINVITE command which lets users un-invite other users from channels (!) */

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "message.h"

static Server *Srv;
	 
class cmd_uninvite : public command_t
{
 public:
	cmd_uninvite () : command_t("UNINVITE", 0, 2)
	{
		this->source = "m_uninvite.so";
		syntax = "<nick> <channel>";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* u = Find(parameters[0]);
		chanrec* c = FindChan(parameters[1]);
			 
		if ((!c) || (!u))
		{	
			if (!c)
			{
				WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[1]);
			}
			else
			{
				WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
			}
				
			return; 
		}	

		if (c->modes[CM_INVITEONLY])
		{
			if (cstatus(user,c) < STATUS_HOP)
			{
				WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, c->name);
				return;
			}
		}

		irc::string xname(c->name);

		if (!u->IsInvited(xname))
		{
			WriteServ(user->fd,"491 %s %s %s :Is not invited to channel %s",user->nick,u->nick,c->name,c->name);
			return;
		}
		if (!c->HasUser(user))
		{
			WriteServ(user->fd,"492 %s %s :You're not on that channel!",user->nick, c->name);
			return;
		}

		u->RemoveInvite(xname);
		WriteServ(user->fd,"494 %s %s %s :Uninvited",user->nick,c->name,u->nick);
		WriteServ(u->fd,"493 %s :You were uninvited from %s by %s",u->nick,c->name,user->nick);
		c->WriteChannelWithServ(Srv->GetServerName().c_str(), "NOTICE %s :*** %s uninvited %s.", c->name, user->nick, u->nick);
	}
};

class ModuleUninvite : public Module
{
	cmd_uninvite *mycommand;

 public:

	ModuleUninvite(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_uninvite();
		Srv->AddCommand(mycommand);
	}
	
	virtual ~ModuleUninvite()
	{
	}
	
	virtual Version GetVersion()
	{
		/* Must be static, because we dont want to desync invite lists */
		return Version(1, 0, 0, 0, VF_VENDOR|VF_STATIC);
	}
};


class ModuleUninviteFactory : public ModuleFactory
{
 public:
	ModuleUninviteFactory()
	{
	}
	
	~ModuleUninviteFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleUninvite(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleUninviteFactory;
}
