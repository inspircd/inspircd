/* Support for a dancer-style /remove command, an alternative to /kick to try and avoid auto-rejoin-on-kick scripts */
/* Written by Om, 25-03-05 */

using namespace std;

#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides a /remove command, this is mostly an alternative to /kick, except makes users appear to have parted the channel */

/*	
 * This module supports the use of the +q and +a usermodes, but should work without them too.
 * Usage of the command is restricted to +hoaq, and you cannot remove a user with a "higher" level than yourself.
 * eg: +h can remove +hv and users with no modes. +a can remove +aohv and users with no modes.
*/

static Server *Srv;

/* This little function just converts a chanmode character (~ & @ & +) into an integer (5 4 3 2 1) */
/* XXX - this could be handy in the core, so it can be used elsewhere */
int chartolevel(std::string &privs)
{
	const char* n = privs.c_str();

	switch (*n)
	{
		case '~':
			return 5;
		break;
		case '&':
			return 4;
		break;
		case '@':
			return 3;
		break;
		case '%':
			return 2;
		break;
		default:
			return 1;
		break;
	}
	return 1;
}

class cmd_remove : public command_t
{
 public:
	cmd_remove () : command_t("REMOVE", 0, 2)
	{
		this->source = "m_remove.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		userrec* target;
		chanrec* channel;
		int tlevel, ulevel;
		char* dummy;
		std::string tprivs, uprivs, reason;
		
		
		/* Look up the user we're meant to be removing from the channel */
		target = Srv->FindNick(parameters[0]);
		
		/* And the channel we're meant to be removing them from */
		channel = Srv->FindChannel(parameters[1]);

		/* Fix by brain - someone needs to learn to validate their input! */
		if (!target || !channel)
		{
			WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, !target ? parameters[0] : parameters[1]);
			return;
		}

		if (!channel->HasUser(target))
		{
			Srv->SendTo(NULL,user,"NOTICE "+std::string(user->nick)+" :*** The user "+target->nick+" is not on channel "+channel->name);
			return;
		}	

		/* And see if the person calling the command has access to use it on the channel */
		uprivs = Srv->ChanMode(user, channel);
		
		/* Check what privs the person being removed has */
		tprivs = Srv->ChanMode(target, channel);

		if(pcnt > 2)
			reason = "Removed by " + std::string(user->nick) + ":";
		else
			reason = "Removed by " + std::string(user->nick);
		
		/* This turns all the parameters after the first two into a single string, so the part reason can be multi-word */
		for (int n = 2; n < pcnt; n++)
		{
			reason += " ";
			reason += parameters[n];
		}
		
		/* This is adding support for the +q and +a channel modes, basically if they are enabled, and the remover has them set. */
		/* Then we change the @|%|+ to & if they are +a, or ~ if they are +q */

		std::string protect = "cm_protect_" + std::string(channel->name);
		std::string founder = "cm_founder_"+std::string(channel->name);
		
		if (user->GetExt(protect, dummy))
			uprivs = "&";
		if (user->GetExt(founder, dummy))
			uprivs = "~";
			
		/* Now it's the same idea, except for the target */
		if (target->GetExt(protect, dummy))
			tprivs = "&";
		if (target->GetExt(founder, dummy))
			tprivs = "~";
			
		tlevel = chartolevel(tprivs);
		ulevel = chartolevel(uprivs);
		
		/* If the user calling the command is either an admin, owner, operator or a half-operator on the channel */
		if (ulevel > 1)
		{
			/* For now, we'll let everyone remove their level and below, eg ops can remove ops, halfops, voices, and those with no mode (no moders actually are set to 1) */
			if ((ulevel >= tlevel && tlevel != 5) && (!Srv->IsUlined(target->server)))
			{
				Srv->PartUserFromChannel(target, channel->name, reason);
				WriteServ(user->fd, "NOTICE %s :%s removed %s from the channel", channel->name, user->nick, target->nick);
				WriteServ(target->fd, "NOTICE %s :*** %s removed you from %s with the message:%s", target->nick, user->nick, channel->name, reason.c_str());
			}
			else
			{
				WriteServ(user->fd, "NOTICE %s :*** You do not have access to /remove %s from %s", user->nick, target->nick, channel->name);
			}
		}
		else
		{
			WriteServ(user->fd, "NOTICE %s :*** You do not have access to use /remove on %s", user->nick, channel->name);
		}
	}
};

class ModuleRemove : public Module
{
	cmd_remove* mycommand;
 public:
	ModuleRemove(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_remove();
		Srv->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		output = output + std::string(" REMOVE");
	}
	
	virtual ~ModuleRemove()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleRemoveFactory : public ModuleFactory
{
 public:
	ModuleRemoveFactory()
	{
	}
	
	~ModuleRemoveFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleRemove(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleRemoveFactory;
}
