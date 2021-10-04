/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2018, 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Justin Crawford <Justasic@Gmail.com>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
 *   Copyright (C) 2005-2006, 2010 Craig Edwards <brain@inspircd.org>
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
#include "modules/isupport.h"

/*
 * This module supports the use of the +q and +a usermodes, but should work without them too.
 * Usage of the command is restricted to +hoaq, and you cannot remove a user with a "higher" level than yourself.
 * eg: +h can remove +hv and users with no modes. +a can remove +aohv and users with no modes.
*/

/** Base class for /FPART and /REMOVE
 */
class RemoveBase
	: public Command
{
	bool& supportnokicks;
	ChanModeReference& nokicksmode;

 public:
	unsigned long protectedrank;

	RemoveBase(Module* Creator, bool& snk, ChanModeReference& nkm, const char* cmdn)
		: Command(Creator, cmdn, 2, 3)
		, supportnokicks(snk)
		, nokicksmode(nkm)
	{
	}

	CmdResult HandleRMB(User* user, const CommandBase::Params& parameters,  bool fpart)
	{
		User* target;
		Channel* channel;
		std::string reason;

		// If the command is a /REMOVE then detect the parameter order
		bool neworder = (fpart || ServerInstance->Channels.IsPrefix(parameters[0][0]));

		/* Set these to the parameters needed, the new version of this module switches it's parameters around
		 * supplying a new command with the new order while keeping the old /remove with the older order.
		 * /remove <nick> <channel> [reason ...]
		 * /fpart <channel> <nick> [reason ...]
		 */
		const std::string& channame = parameters[neworder ? 0 : 1];
		const std::string& username = parameters[neworder ? 1 : 0];

		/* Look up the user we're meant to be removing from the channel */
		if (IS_LOCAL(user))
			target = ServerInstance->Users.FindNick(username);
		else
			target = ServerInstance->Users.Find(username);

		/* And the channel we're meant to be removing them from */
		channel = ServerInstance->Channels.Find(channame);

		/* Fix by brain - someone needs to learn to validate their input! */
		if (!channel)
		{
			user->WriteNumeric(Numerics::NoSuchChannel(channame));
			return CmdResult::FAILURE;
		}
		if ((!target) || (target->registered != REG_ALL))
		{
			user->WriteNumeric(Numerics::NoSuchNick(username));
			return CmdResult::FAILURE;
		}

		if (!channel->HasUser(target))
		{
			user->WriteNotice(InspIRCd::Format("*** User %s is not on channel %s", target->nick.c_str(), channel->name.c_str()));
			return CmdResult::FAILURE;
		}

		if (target->server->IsService())
		{
			user->WriteNumeric(ERR_CHANOPRIVSNEEDED, channame, "Only a service may remove a service from a channel.");
			return CmdResult::FAILURE;
		}

		/* We support the +Q channel mode via. the m_nokicks module, if the module is loaded and the mode is set then disallow the /remove */
		if ((!IS_LOCAL(user)) || (!supportnokicks) || (!channel->IsModeSet(nokicksmode)))
		{
			/* We'll let everyone remove their level and below, eg:
			 * ops can remove ops, halfops, voices, and those with no mode (no moders actually are set to 1)
			 * a services target will get a higher level than it's possible for a /remover to get..so they're safe.
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
					CommandBase::Params p;
					p.push_back(target->uuid);
					p.push_back(channel->name);
					if (parameters.size() > 2)
						p.push_back(":" + parameters[2]);
					ServerInstance->PI->SendEncapsulatedData(target->server->GetName(), "REMOVE", p, user);

					return CmdResult::SUCCESS;
				}

				std::string reasonparam;

				/* If a reason is given, use it */
				if(parameters.size() > 2)
					reasonparam = parameters[2];
				else
					reasonparam = "No reason given";

				/* Build up the part reason string. */
				reason = "Removed by " + user->nick + ": " + reasonparam;

				channel->WriteRemoteNotice(InspIRCd::Format("%s removed %s from the channel", user->nick.c_str(), target->nick.c_str()));
				target->WriteNotice("*** " + user->nick + " removed you from " + channel->name + " with the message: " + reasonparam);

				channel->PartUser(target, reason);
			}
			else
			{
				user->WriteNotice(InspIRCd::Format("*** You do not have access to /REMOVE %s from %s", target->nick.c_str(), channel->name.c_str()));
				return CmdResult::FAILURE;
			}
		}
		else
		{
			/* m_nokicks.so was loaded and +Q was set, block! */
			user->WriteNumeric(ERR_RESTRICTED, channel->name, InspIRCd::Format("Can't remove user %s from channel (+Q is set)", target->nick.c_str()));
			return CmdResult::FAILURE;
		}

		return CmdResult::SUCCESS;
	}
};

class CommandRemove final
	: public RemoveBase
{
 public:
	CommandRemove(Module* Creator, bool& snk, ChanModeReference& nkm)
		: RemoveBase(Creator, snk, nkm, "REMOVE")
	{
		syntax = { "<channel> <nick> [:<reason>]" };
		translation = { TR_NICK, TR_TEXT, TR_TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		return HandleRMB(user, parameters, false);
	}
};

class CommandFpart final
	: public RemoveBase
{
 public:
	CommandFpart(Module* Creator, bool& snk, ChanModeReference& nkm)
		: RemoveBase(Creator, snk, nkm, "FPART")
	{
		syntax = { "<channel> <nick> [:<reason>]" };
		translation = { TR_TEXT, TR_NICK, TR_TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		return HandleRMB(user, parameters, true);
	}
};

class ModuleRemove final
	: public Module
	, public ISupport::EventListener
{
 private:
	ChanModeReference nokicksmode;
	CommandRemove cmd1;
	CommandFpart cmd2;
	bool supportnokicks;

 public:
	ModuleRemove()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /FPART and /REMOVE commands which allows channel operators to force part users from a channel.")
		, ISupport::EventListener(this)
		, nokicksmode(this, "nokick")
		, cmd1(this, supportnokicks, nokicksmode)
		, cmd2(this, supportnokicks, nokicksmode)
	{
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["REMOVE"];
	}

	void ReadConfig(ConfigStatus& status) override
	{
		auto tag = ServerInstance->Config->ConfValue("remove");
		supportnokicks = tag->getBool("supportnokicks");
		cmd1.protectedrank = cmd2.protectedrank = tag->getUInt("protectedrank", 50000);
	}
};

MODULE_INIT(ModuleRemove)
