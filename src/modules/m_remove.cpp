/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
class RemoveBase : public Command
{
 private:
	bool& supportnokicks;

 public:
	RemoveBase(Module* Creator, bool& snk, const char* cmdn)
		: Command(Creator, cmdn, 2, 3), supportnokicks(snk)
	{
	}

	CmdResult HandleRMB(const std::vector<std::string>& parameters, User *user, bool neworder)
	{
		User* target;
		Channel* channel;
		std::string reason;
		std::string protectkey;
		std::string founderkey;
		bool hasnokicks;

		/* Set these to the parameters needed, the new version of this module switches it's parameters around
		 * supplying a new command with the new order while keeping the old /remove with the older order.
		 * /remove <nick> <channel> [reason ...]
		 * /fpart <channel> <nick> [reason ...]
		 */
		const std::string& channame = parameters[neworder ? 0 : 1];
		const std::string& username = parameters[neworder ? 1 : 0];

		/* Look up the user we're meant to be removing from the channel */
		if (IS_LOCAL(user))
			target = ServerInstance->FindNickOnly(username);
		else
			target = ServerInstance->FindNick(username);

		/* And the channel we're meant to be removing them from */
		channel = ServerInstance->FindChan(channame);

		/* Fix by brain - someone needs to learn to validate their input! */
		if ((!target) || (target->registered != REG_ALL) || (!channel))
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), !channel ? channame.c_str() : username.c_str());
			return CMD_FAILURE;
		}

		if (!channel->HasUser(target))
		{
			user->WriteServ( "NOTICE %s :*** The user %s is not on channel %s", user->nick.c_str(), target->nick.c_str(), channel->name.c_str());
			return CMD_FAILURE;
		}

		int ulevel = channel->GetPrefixValue(user);
		int tlevel = channel->GetPrefixValue(target);

		hasnokicks = (ServerInstance->Modules->Find("m_nokicks.so") && channel->IsModeSet('Q'));

		if (ServerInstance->ULine(target->server))
		{
			user->WriteNumeric(482, "%s %s :Only a u-line may remove a u-line from a channel.", user->nick.c_str(), channame.c_str());
			return CMD_FAILURE;
		}

		/* We support the +Q channel mode via. the m_nokicks module, if the module is loaded and the mode is set then disallow the /remove */
		if ((!IS_LOCAL(user)) || (!supportnokicks || !hasnokicks))
		{
			/* We'll let everyone remove their level and below, eg:
			 * ops can remove ops, halfops, voices, and those with no mode (no moders actually are set to 1)
			 * a ulined target will get a higher level than it's possible for a /remover to get..so they're safe.
			 * Nobody may remove a founder.
			 */
			if ((!IS_LOCAL(user)) || ((ulevel > VOICE_VALUE) && (ulevel >= tlevel) && (tlevel != 50000)))
			{
				// REMOVE/FPART will be sent to the target's server and it will reply with a PART (or do nothing if it doesn't understand the command)
				if (!IS_LOCAL(target))
					return CMD_SUCCESS;

				std::string reasonparam;

				/* If a reason is given, use it */
				if(parameters.size() > 2)
					reasonparam = parameters[2];
				else
					reasonparam = "No reason given";

				/* Build up the part reason string. */
				reason = "Removed by " + user->nick + ": " + reasonparam;

				channel->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s removed %s from the channel", channel->name.c_str(), user->nick.c_str(), target->nick.c_str());
				target->WriteServ("NOTICE %s :*** %s removed you from %s with the message: %s", target->nick.c_str(), user->nick.c_str(), channel->name.c_str(), reasonparam.c_str());

				channel->PartUser(target, reason);
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

		return CMD_SUCCESS;
	}
	virtual RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) = 0;
};

/** Handle /REMOVE
 */
class CommandRemove : public RemoveBase
{
 public:
	CommandRemove(Module* Creator, bool& snk)
		: RemoveBase(Creator, snk, "REMOVE")
	{
		syntax = "<nick> <channel> [<reason>]";
		TRANSLATE4(TR_NICK, TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		return HandleRMB(parameters, user, false);
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
			return ROUTE_OPT_UCAST(dest->server);
		return ROUTE_LOCALONLY;
	}
};

/** Handle /FPART
 */
class CommandFpart : public RemoveBase
{
 public:
	CommandFpart(Module* Creator, bool& snk)
		: RemoveBase(Creator, snk, "FPART")
	{
		syntax = "<channel> <nick> [<reason>]";
		TRANSLATE4(TR_TEXT, TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		return HandleRMB(parameters, user, true);
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* dest = ServerInstance->FindNick(parameters[1]);
		if (dest)
			return ROUTE_OPT_UCAST(dest->server);
		return ROUTE_LOCALONLY;
	}
};

class ModuleRemove : public Module
{
	CommandRemove cmd1;
	CommandFpart cmd2;
	bool supportnokicks;


 public:
	ModuleRemove() : cmd1(this, supportnokicks), cmd2(this, supportnokicks)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd1);
		ServerInstance->Modules->AddService(cmd2);
		OnRehash(NULL);
		Implementation eventlist[] = { I_On005Numeric, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" REMOVE");
	}

	virtual void OnRehash(User* user)
	{
		supportnokicks = ServerInstance->Config->ConfValue("remove")->getBool("supportnokicks");
	}

	virtual ~ModuleRemove()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides a /remove command, this is mostly an alternative to /kick, except makes users appear to have parted the channel", VF_OPTCOMMON | VF_VENDOR);
	}

};

MODULE_INIT(ModuleRemove)
