/* Support for a dancer-style /remove command, an alternative to /kick to try and avoid auto-rejoin-on-kick scripts */
/* Written by Om, 25-03-05 */

#include <sstream>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "inspircd.h"

/* $ModDesc: Provides a /remove command, this is mostly an alternative to /kick, except makes users appear to have parted the channel */

/*	
 * This module supports the use of the +q and +a usermodes, but should work without them too.
 * Usage of the command is restricted to +hoaq, and you cannot remove a user with a "higher" level than yourself.
 * eg: +h can remove +hv and users with no modes. +a can remove +aohv and users with no modes.
*/

extern InspIRCd* ServerInstance;

class RemoveBase
{
 private: 
	Server* Srv;
	bool& supportnokicks;
 
 protected:
	RemoveBase(Server* Me, bool& snk)
	: Srv(Me), supportnokicks(snk)
	{
	}		
 
	enum ModeLevel { PEON = 0, HALFOP = 1, OP = 2, ADMIN = 3, OWNER = 4, ULINE = 5 };	 
 
	/* This little function just converts a chanmode character (U ~ & @ & +) into an integer (5 4 3 2 1 0) */
	/* XXX - this could be handy in the core, so it can be used elsewhere */
	ModeLevel chartolevel(const std::string &privs)
	{
		if(privs.empty())
		{
			return PEON;
		}
	
		switch (privs[0])
		{
			case 'U':
				/* Ulined */
				return ULINE;
			case '~':
				/* Owner */
				return OWNER;
			case '&':
				/* Admin */
				return ADMIN;
			case '@':
				/* Operator */
				return OP;
			case '%':
				/* Halfop */
				return HALFOP;
			default:
				/* Peon */
				return PEON;
		}
	}
	
	void Handle (const char** parameters, int pcnt, userrec *user, bool neworder)
	{
		const char* channame;
		const char* username;
		userrec* target;
		chanrec* channel;
		ModeLevel tlevel;
		ModeLevel ulevel;
		std::ostringstream reason;
		std::string protectkey;
		std::string founderkey;
		bool hasnokicks;
		
		/* Set these to the parameters needed, the new version of this module switches it's parameters around
		 * supplying a new command with the new order while keeping the old /remove with the older order.
		 * /remove <nick> <channel> [reason ...]
		 * /fpart <channel> <nick> [reason ...]
		 */
		channame = parameters[ neworder ? 0 : 1];
		username = parameters[ neworder ? 1 : 0];
		
		/* Look up the user we're meant to be removing from the channel */
		target = ServerInstance->FindNick(username);
		
		/* And the channel we're meant to be removing them from */
		channel = ServerInstance->FindChan(channame);

		/* Fix by brain - someone needs to learn to validate their input! */
		if (!target || !channel)
		{
			user->WriteServ("401 %s %s :No such nick/channel", user->nick, !target ? username : channame);
			return;
		}

		if (!channel->HasUser(target))
		{
			user->WriteServ( "NOTICE %s :*** The user %s is not on channel %s", user->nick, target->nick, channel->name);
			return;
		}	
		
		/* This is adding support for the +q and +a channel modes, basically if they are enabled, and the remover has them set.
		 * Then we change the @|%|+ to & if they are +a, or ~ if they are +q */
		protectkey = "cm_protect_" + std::string(channel->name);
		founderkey = "cm_founder_" + std::string(channel->name);
		
		if (Srv->IsUlined(user->server) || Srv->IsUlined(user->nick))
		{
			log(DEBUG, "Setting ulevel to U");
			ulevel = chartolevel("U");
		}
		if (user->GetExt(founderkey))
		{
			log(DEBUG, "Setting ulevel to ~");
			ulevel = chartolevel("~");
		}
		else if (user->GetExt(protectkey))
		{
			log(DEBUG, "Setting ulevel to &");
			ulevel = chartolevel("&");
		}
		else
		{
			log(DEBUG, "Setting ulevel to %s", Srv->ChanMode(user, channel).c_str());
			ulevel = chartolevel(Srv->ChanMode(user, channel));
		}
			
		/* Now it's the same idea, except for the target. If they're ulined make sure they get a higher level than the sender can */
		if (Srv->IsUlined(target->server) || Srv->IsUlined(target->nick))
		{
			log(DEBUG, "Setting tlevel to U");
			tlevel = chartolevel("U");
		}
		else if (target->GetExt(founderkey))
		{
			log(DEBUG, "Setting tlevel to ~");
			tlevel = chartolevel("~");
		}
		else if (target->GetExt(protectkey))
		{
			log(DEBUG, "Setting tlevel to &");
			tlevel = chartolevel("&");
		}
		else
		{
			log(DEBUG, "Setting tlevel to %s", Srv->ChanMode(target, channel).c_str());
			tlevel = chartolevel(Srv->ChanMode(target, channel));
		}
		
		hasnokicks = (Srv->FindModule("m_nokicks.so") && channel->IsModeSet('Q'));
		
		/* We support the +Q channel mode via. the m_nokicks module, if the module is loaded and the mode is set then disallow the /remove */
		if(!supportnokicks || !hasnokicks || (ulevel == ULINE))
		{
			/* We'll let everyone remove their level and below, eg:
			 * ops can remove ops, halfops, voices, and those with no mode (no moders actually are set to 1)
			 * a ulined target will get a higher level than it's possible for a /remover to get..so they're safe.
			 * Nobody may remove a founder.
			 */
			if ((ulevel > PEON) && (ulevel >= tlevel) && (tlevel != OWNER))
			{
				std::string reasonparam;
				
				/* If a reason is given, use it */
				if(pcnt > 2)
				{
					 reason <<  ":";
					
					/* Use all the remaining parameters as the reason */
					for(int i = 2; i < pcnt; i++)
					{
						reason << " " << parameters[i];
					}
					
					reasonparam = reason.str();
					reason.clear();
				}

				/* Build up the part reason string. */
				reason << "Removed by " << user->nick << reasonparam;

				channel->WriteChannelWithServ(Srv->GetServerName().c_str(), "NOTICE %s :%s removed %s from the channel", channel->name, user->nick, target->nick);
				target->WriteServ("NOTICE %s :*** %s removed you from %s with the message: %s", target->nick, user->nick, channel->name, reasonparam.c_str());

				if (!channel->PartUser(target, reason.str().c_str()))
					delete channel;
			}
			else
			{
				user->WriteServ( "NOTICE %s :*** You do not have access to /remove %s from %s", user->nick, target->nick, channel->name);
			}
		}
		else
		{
			/* m_nokicks.so was loaded and +Q was set, block! */
			user->WriteServ( "484 %s %s :Can't remove user %s from channel (+Q set)", user->nick, channel->name, target->nick);
		}
	}
};

class cmd_remove : public command_t, public RemoveBase
{
 public:
	cmd_remove(Server* Srv, bool& snk) : command_t("REMOVE", 0, 2), RemoveBase(Srv, snk)
	{
		this->source = "m_remove.so";
		syntax = "<nick> <channel> [<reason>]";
	}
	
	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		RemoveBase::Handle(parameters, pcnt, user, false);
	}
};

class cmd_fpart : public command_t, public RemoveBase
{
 public:
	cmd_fpart(Server* Srv, bool snk) : command_t("FPART", 0, 2), RemoveBase(Srv, snk)
	{
		this->source = "m_remove.so";
		syntax = "<channel> <nick> [<reason>]";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		RemoveBase::Handle(parameters, pcnt, user, true);
	}	
};

class ModuleRemove : public Module
{
	cmd_remove* mycommand;
	cmd_fpart* mycommand2;
	bool supportnokicks;
	
 public:
	ModuleRemove(Server* Me)
	: Module::Module(Me)
	{
		mycommand = new cmd_remove(Me, supportnokicks);
		mycommand2 = new cmd_fpart(Me, supportnokicks);
		Me->AddCommand(mycommand);
		Me->AddCommand(mycommand2);
		OnRehash("");
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnRehash] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" REMOVE");
	}
	
	virtual void OnRehash(const std::string&)
	{
		ConfigReader conf;
		
		supportnokicks = conf.ReadFlag("remove", "supportnokicks", 0);
	}
	
	virtual ~ModuleRemove()
	{
		delete mycommand;
		delete mycommand2;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,1,0,VF_VENDOR);
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
