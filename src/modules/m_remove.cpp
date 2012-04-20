/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005-2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


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

	CmdResult Handle (const std::vector<std::string>& parameters, User *user, bool neworder)
	{
		const char* channame;
		const char* username;
		User* target;
		Channel* channel;
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
		channame = parameters[ neworder ? 0 : 1].c_str();
		username = parameters[ neworder ? 1 : 0].c_str();

		/* Look up the user we're meant to be removing from the channel */
		target = ServerInstance->FindNick(username);

		/* And the channel we're meant to be removing them from */
		channel = ServerInstance->FindChan(channame);

		/* Fix by brain - someone needs to learn to validate their input! */
		if (!target || !channel)
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), !target ? username : channame);
			return CMD_FAILURE;
		}

		if (!channel->HasUser(target))
		{
			user->WriteServ( "NOTICE %s :*** The user %s is not on channel %s", user->nick.c_str(), target->nick.c_str(), channel->name.c_str());
			return CMD_FAILURE;
		}

		/* This is adding support for the +q and +a channel modes, basically if they are enabled, and the remover has them set.
		 * Then we change the @|%|+ to & if they are +a, or ~ if they are +q */
		protectkey = "cm_protect_" + std::string(channel->name);
		founderkey = "cm_founder_" + std::string(channel->name);

		if (ServerInstance->ULine(user->server) || ServerInstance->ULine(user->nick.c_str()))
		{
			ulevel = chartolevel("U");
		}
		if (user->GetExt(founderkey))
		{
			ulevel = chartolevel("~");
		}
		else if (user->GetExt(protectkey))
		{
			ulevel = chartolevel("&");
		}
		else
		{
			ulevel = chartolevel(channel->GetPrefixChar(user));
		}

		/* Now it's the same idea, except for the target. If they're ulined make sure they get a higher level than the sender can */
		if (ServerInstance->ULine(target->server) || ServerInstance->ULine(target->nick.c_str()))
		{
			tlevel = chartolevel("U");
		}
		else if (target->GetExt(founderkey))
		{
			tlevel = chartolevel("~");
		}
		else if (target->GetExt(protectkey))
		{
			tlevel = chartolevel("&");
		}
		else
		{
			tlevel = chartolevel(channel->GetPrefixChar(target));
		}

		hasnokicks = (ServerInstance->Modules->Find("m_nokicks.so") && channel->IsModeSet('Q'));

		/* We support the +Q channel mode via. the m_nokicks module, if the module is loaded and the mode is set then disallow the /remove */
		if ((!IS_LOCAL(user)) || (!supportnokicks || !hasnokicks || (ulevel == ULINE)))
		{
			/* We'll let everyone remove their level and below, eg:
			 * ops can remove ops, halfops, voices, and those with no mode (no moders actually are set to 1)
			 * a ulined target will get a higher level than it's possible for a /remover to get..so they're safe.
			 * Nobody may remove a founder.
			 */
			if ((!IS_LOCAL(user)) || ((ulevel > PEON) && (ulevel >= tlevel) && (tlevel != OWNER)))
			{
				// no you can't just go from a std::ostringstream to a std::string, Om. -nenolod
				// but you can do this, nenolod -brain

				std::string reasonparam("No reason given");

				/* If a reason is given, use it */
				if(parameters.size() > 2)
				{
					/* Join params 2 ... pcnt - 1 (inclusive) into one */
					irc::stringjoiner reason_join(" ", parameters, 2, parameters.size() - 1);
					reasonparam = reason_join.GetJoined();
				}

				/* Build up the part reason string. */
				reason = std::string("Removed by ") + user->nick + ": " + reasonparam;

				channel->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s removed %s from the channel", channel->name.c_str(), user->nick.c_str(), target->nick.c_str());
				target->WriteServ("NOTICE %s :*** %s removed you from %s with the message: %s", target->nick.c_str(), user->nick.c_str(), channel->name.c_str(), reasonparam.c_str());

				if (!channel->PartUser(target, reason))
					delete channel;
			}
			else
			{
				user->WriteServ( "NOTICE %s :*** You do not have access to /remove %s from %s", user->nick.c_str(), target->nick.c_str(), channel->name.c_str());
				return CMD_FAILURE;
			}
		}
		else
		{
			/* m_nokicks.so was loaded and +Q was set, block! */
			user->WriteServ( "484 %s %s :Can't remove user %s from channel (+Q set)", user->nick.c_str(), channel->name.c_str(), target->nick.c_str());
			return CMD_FAILURE;
		}

		/* route me */
		return CMD_SUCCESS;
	}
};

/** Handle /REMOVE
 */
class CommandRemove : public Command, public RemoveBase
{
 public:
	CommandRemove(InspIRCd* Instance, bool& snk) : Command(Instance, "REMOVE", 0, 2, 3, false, 0), RemoveBase(Instance, snk)
	{
		this->source = "m_remove.so";
		syntax = "<nick> <channel> [<reason>]";
		TRANSLATE4(TR_NICK, TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		return RemoveBase::Handle(parameters, user, false);
	}
};

/** Handle /FPART
 */
class CommandFpart : public Command, public RemoveBase
{
 public:
	CommandFpart(InspIRCd* Instance, bool& snk) : Command(Instance, "FPART", 0, 2, 3), RemoveBase(Instance, snk)
	{
		this->source = "m_remove.so";
		syntax = "<channel> <nick> [<reason>]";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		return RemoveBase::Handle(parameters, user, true);
	}
};

class ModuleRemove : public Module
{
	CommandRemove* mycommand;
	CommandFpart* mycommand2;
	bool supportnokicks;


 public:
	ModuleRemove(InspIRCd* Me)
	: Module(Me)
	{
		mycommand = new CommandRemove(ServerInstance, supportnokicks);
		mycommand2 = new CommandFpart(ServerInstance, supportnokicks);
		ServerInstance->AddCommand(mycommand);
		ServerInstance->AddCommand(mycommand2);
		OnRehash(NULL);
		Implementation eventlist[] = { I_On005Numeric, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}


	virtual void On005Numeric(std::string &output)
	{
		output.append(" REMOVE");
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader conf(ServerInstance);
		supportnokicks = conf.ReadFlag("remove", "supportnokicks", 0);
	}

	virtual ~ModuleRemove()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleRemove)
