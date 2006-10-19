/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                     E-mail:
 *              <brain@chatspike.net>
 *              <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 * the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* Support for a dancer-style /remove command, an alternative to /kick to try and avoid auto-rejoin-on-kick scripts */
/* Written by Om, 25-03-05 */

#include <sstream>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "configreader.h"
#include "inspircd.h"

/* $ModDesc: Provides a /remove command, this is mostly an alternative to /kick, except makes users appear to have parted the channel */

/*	
 * This module supports the use of the +q and +a usermodes, but should work without them too.
 * Usage of the command is restricted to +hoaq, and you cannot remove a user with a "higher" level than yourself.
 * eg: +h can remove +hv and users with no modes. +a can remove +aohv and users with no modes.
*/

/** Base class for /FPART and /REMOVE
 */
class RemoveBase
{
 private: 
	bool& supportnokicks;
	InspIRCd* ServerInstance;
 
 protected:
	RemoveBase(InspIRCd* Instance, bool& snk) : supportnokicks(snk), ServerInstance(Instance)
	{
	}		
 
	enum ModeLevel { PEON = 0, HALFOP = 1, OP = 2, ADMIN = 3, OWNER = 4, ULINE = 5 };	 
 
	/* This little function just converts a chanmode character (U ~ & @ & +) into an integer (5 4 3 2 1 0) */
	/* XXX - We should probably use the new mode prefix rank stuff
	 * for this instead now -- Brain */
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
	
	CmdResult Handle (const char** parameters, int pcnt, userrec *user, bool neworder)
	{
		const char* channame;
		const char* username;
		userrec* target;
		chanrec* channel;
		ModeLevel tlevel;
		ModeLevel ulevel;
		std::string reason;
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
			return CMD_FAILURE;
		}

		if (!channel->HasUser(target))
		{
			user->WriteServ( "NOTICE %s :*** The user %s is not on channel %s", user->nick, target->nick, channel->name);
			return CMD_FAILURE;
		}	
		
		/* This is adding support for the +q and +a channel modes, basically if they are enabled, and the remover has them set.
		 * Then we change the @|%|+ to & if they are +a, or ~ if they are +q */
		protectkey = "cm_protect_" + std::string(channel->name);
		founderkey = "cm_founder_" + std::string(channel->name);
		
		if (ServerInstance->ULine(user->server) || ServerInstance->ULine(user->nick))
		{
			ServerInstance->Log(DEBUG, "Setting ulevel to U");
			ulevel = chartolevel("U");
		}
		if (user->GetExt(founderkey))
		{
			ServerInstance->Log(DEBUG, "Setting ulevel to ~");
			ulevel = chartolevel("~");
		}
		else if (user->GetExt(protectkey))
		{
			ServerInstance->Log(DEBUG, "Setting ulevel to &");
			ulevel = chartolevel("&");
		}
		else
		{
			ServerInstance->Log(DEBUG, "Setting ulevel to %s", channel->GetPrefixChar(user));
			ulevel = chartolevel(channel->GetPrefixChar(user));
		}
			
		/* Now it's the same idea, except for the target. If they're ulined make sure they get a higher level than the sender can */
		if (ServerInstance->ULine(target->server) || ServerInstance->ULine(target->nick))
		{
			ServerInstance->Log(DEBUG, "Setting tlevel to U");
			tlevel = chartolevel("U");
		}
		else if (target->GetExt(founderkey))
		{
			ServerInstance->Log(DEBUG, "Setting tlevel to ~");
			tlevel = chartolevel("~");
		}
		else if (target->GetExt(protectkey))
		{
			ServerInstance->Log(DEBUG, "Setting tlevel to &");
			tlevel = chartolevel("&");
		}
		else
		{
			ServerInstance->Log(DEBUG, "Setting tlevel to %s", channel->GetPrefixChar(target));
			tlevel = chartolevel(channel->GetPrefixChar(target));
		}
		
		hasnokicks = (ServerInstance->FindModule("m_nokicks.so") && channel->IsModeSet('Q'));
		
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
				// no you can't just go from a std::ostringstream to a std::string, Om. -nenolod
				// but you can do this, nenolod -brain

				std::string reasonparam("No reason given");
				
				/* If a reason is given, use it */
				if(pcnt > 2)
				{
					/* Join params 2 ... pcnt - 1 (inclusive) into one */
					irc::stringjoiner reason_join(" ", parameters, 2, pcnt - 1);
					reasonparam = reason_join.GetJoined();
				}

				/* Build up the part reason string. */
				reason = std::string("Removed by ") + user->nick + ": " + reasonparam;

				channel->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s removed %s from the channel", channel->name, user->nick, target->nick);
				target->WriteServ("NOTICE %s :*** %s removed you from %s with the message: %s", target->nick, user->nick, channel->name, reasonparam.c_str());

				if (!channel->PartUser(target, reason.c_str()))
					delete channel;
			}
			else
			{
				user->WriteServ( "NOTICE %s :*** You do not have access to /remove %s from %s", user->nick, target->nick, channel->name);
				return CMD_FAILURE;
			}
		}
		else
		{
			/* m_nokicks.so was loaded and +Q was set, block! */
			user->WriteServ( "484 %s %s :Can't remove user %s from channel (+Q set)", user->nick, channel->name, target->nick);
			return CMD_FAILURE;
		}

		return CMD_SUCCESS;
	}
};

/** Handle /REMOVE
 */
class cmd_remove : public command_t, public RemoveBase
{
 public:
	cmd_remove(InspIRCd* Instance, bool& snk) : command_t(Instance, "REMOVE", 0, 2), RemoveBase(Instance, snk)
	{
		this->source = "m_remove.so";
		syntax = "<nick> <channel> [<reason>]";
	}
	
	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		return RemoveBase::Handle(parameters, pcnt, user, false);
	}
};

/** Handle /FPART
 */
class cmd_fpart : public command_t, public RemoveBase
{
 public:
	cmd_fpart(InspIRCd* Instance, bool& snk) : command_t(Instance, "FPART", 0, 2), RemoveBase(Instance, snk)
	{
		this->source = "m_remove.so";
		syntax = "<channel> <nick> [<reason>]";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		return RemoveBase::Handle(parameters, pcnt, user, true);
	}
};

class ModuleRemove : public Module
{
	cmd_remove* mycommand;
	cmd_fpart* mycommand2;
	bool supportnokicks;
	
	
 public:
	ModuleRemove(InspIRCd* Me)
	: Module::Module(Me)
	{
		mycommand = new cmd_remove(ServerInstance, supportnokicks);
		mycommand2 = new cmd_fpart(ServerInstance, supportnokicks);
		ServerInstance->AddCommand(mycommand);
		ServerInstance->AddCommand(mycommand2);
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
		ConfigReader conf(ServerInstance);
		supportnokicks = conf.ReadFlag("remove", "supportnokicks", 0);
	}
	
	virtual ~ModuleRemove()
	{
		delete mycommand;
		delete mycommand2;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,1,0,VF_VENDOR,API_VERSION);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleRemove(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleRemoveFactory;
}
