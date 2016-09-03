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

/*
 * This module supports the use of the +q and +a usermodes, but should work without them too.
 * Usage of the command is restricted to +hoaq, and you cannot remove a user with a "higher" level than yourself.
 * eg: +h can remove +hv and users with no modes. +a can remove +aohv and users with no modes.
*/

/** Base class for /FPART and /REMOVE
 */
class RemoveBase : public Command
{
	bool& supportnokicks;
	ChanModeReference& nokicksmode;

 public:
 	unsigned int protectedrank;

	RemoveBase(Module* Creator, bool& snk, ChanModeReference& nkm, const char* cmdn)
		: Command(Creator, cmdn, 2, 3)
		, supportnokicks(snk)
		, nokicksmode(nkm)
	{
	}

	CmdResult HandleRMB(const std::vector<std::string>& parameters, User *user, bool fpart)
	{
		User* target;
		Channel* channel;
		std::string reason;

		// If the command is a /REMOVE then detect the parameter order
		bool neworder = ((fpart) || (parameters[0][0] == '#'));

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
			user->WriteNumeric(Numerics::NoSuchNick(channel ? username.c_str() : channame.c_str()));
			return CMD_FAILURE;
		}

		if (!channel->HasUser(target))
		{
			user->WriteNotice(InspIRCd::Format("*** The user %s is not on channel %s", target->nick.c_str(), channel->name.c_str()));
			return CMD_FAILURE;
		}

		if (target->server->IsULine())
		{
			user->WriteNumeric(482, channame, "Only a u-line may remove a u-line from a channel.");
			return CMD_FAILURE;
		}

		/* We support the +Q channel mode via. the m_nokicks module, if the module is loaded and the mode is set then disallow the /remove */
		if ((!IS_LOCAL(user)) || (!supportnokicks) || (!channel->IsModeSet(nokicksmode)))
		{
			/* We'll let everyone remove their level and below, eg:
			 * ops can remove ops, halfops, voices, and those with no mode (no moders actually are set to 1)
			 * a ulined target will get a higher level than it's possible for a /remover to get..so they're safe.
			 * Nobody may remove people with >= protectedrank rank.
			 */
			unsigned int ulevel = channel->GetPrefixValue(user);
			unsigned int tlevel = channel->GetPrefixValue(target);
			if ((!IS_LOCAL(user)) || ((ulevel > VOICE_VALUE) && (ulevel >= tlevel) && ((protectedrank == 0) || (tlevel < protectedrank))))
			{
				// REMOVE will be sent to the target's server and it will reply with a PART (or do nothing if it doesn't understand the command)
				if (!IS_LOCAL(target))
				{
					// Send an ENCAP REMOVE with parameters being in the old <user> <chan> order which is
					// compatible with both 2.0 and 3.0. This also turns FPART into REMOVE.
					std::vector<std::string> p;
					p.push_back(target->uuid);
					p.push_back(channel->name);
					if (parameters.size() > 2)
						p.push_back(":" + parameters[2]);
					ServerInstance->PI->SendEncapsulatedData(target->server->GetName(), "REMOVE", p, user);

					return CMD_SUCCESS;
				}

				std::string reasonparam;

				/* If a reason is given, use it */
				if(parameters.size() > 2)
					reasonparam = parameters[2];
				else
					reasonparam = "No reason given";

				/* Build up the part reason string. */
				reason = "Removed by " + user->nick + ": " + reasonparam;

				channel->WriteNotice(InspIRCd::Format("%s removed %s from the channel", user->nick.c_str(), target->nick.c_str()));
				target->WriteNotice("*** " + user->nick + " removed you from " + channel->name + " with the message: " + reasonparam);

				channel->PartUser(target, reason);
			}
			else
			{
				user->WriteNotice(InspIRCd::Format("*** You do not have access to /remove %s from %s", target->nick.c_str(), channel->name.c_str()));
				return CMD_FAILURE;
			}
		}
		else
		{
			/* m_nokicks.so was loaded and +Q was set, block! */
			user->WriteNumeric(ERR_RESTRICTED, channel->name, InspIRCd::Format("Can't remove user %s from channel (nokicks mode is set)", target->nick.c_str()));
			return CMD_FAILURE;
		}

		return CMD_SUCCESS;
	}
};

/** Handle /REMOVE
 */
class CommandRemove : public RemoveBase
{
 public:
	CommandRemove(Module* Creator, bool& snk, ChanModeReference& nkm)
		: RemoveBase(Creator, snk, nkm, "REMOVE")
	{
		syntax = "<channel> <nick> [<reason>]";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_TEXT);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		return HandleRMB(parameters, user, false);
	}
};

/** Handle /FPART
 */
class CommandFpart : public RemoveBase
{
 public:
	CommandFpart(Module* Creator, bool& snk, ChanModeReference& nkm)
		: RemoveBase(Creator, snk, nkm, "FPART")
	{
		syntax = "<channel> <nick> [<reason>]";
		TRANSLATE3(TR_TEXT, TR_NICK, TR_TEXT);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		return HandleRMB(parameters, user, true);
	}
};

class ModuleRemove : public Module
{
	ChanModeReference nokicksmode;
	CommandRemove cmd1;
	CommandFpart cmd2;
	bool supportnokicks;

 public:
	ModuleRemove()
		: nokicksmode(this, "nokick")
		, cmd1(this, supportnokicks, nokicksmode)
		, cmd2(this, supportnokicks, nokicksmode)
	{
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["REMOVE"];
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("remove");
		supportnokicks = tag->getBool("supportnokicks");
		cmd1.protectedrank = cmd2.protectedrank = tag->getInt("protectedrank", 50000);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides a /remove command, this is mostly an alternative to /kick, except makes users appear to have parted the channel", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleRemove)
