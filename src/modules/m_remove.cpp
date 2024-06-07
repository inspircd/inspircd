/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Justin Crawford <Justasic@Gmail.com>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
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
#include "numerichelper.h"

class CommandRemove final
	: public Command
{
private:
	ChanModeReference nokicksmode;
	UserModeReference servprotectmode;

public:
	ModeHandler::Rank protectedrank;
	bool supportnokicks;

	CommandRemove(Module* Creator)
		: Command(Creator, "REMOVE", 2, 3)
		, nokicksmode(Creator, "nokick")
		, servprotectmode(Creator, "servprotect")
	{
		syntax = { "<channel> <nick> [:<reason>]" };
		translation = { TR_TEXT, TR_NICK, TR_TEXT };
	}

	CmdResult Handle(User* user, const CommandBase::Params& parameters) override
	{
		// Keep compatibility with v3 servers by allowing them to send removes with the old order.
		bool neworder = !IS_LOCAL(user) && ServerInstance->Channels.IsPrefix(parameters[0][0]);
		const std::string& channame = parameters[neworder ? 0 : 1];
		const std::string& username = parameters[neworder ? 1 : 0];

		/* Look up the user we're meant to be removing from the channel */
		User* target;
		if (IS_LOCAL(user))
			target = ServerInstance->Users.FindNick(username, true);
		else
			target = ServerInstance->Users.Find(username, true);

		/* And the channel we're meant to be removing them from */
		auto* channel = ServerInstance->Channels.Find(channame);

		/* Fix by brain - someone needs to learn to validate their input! */
		if (!channel)
		{
			user->WriteNumeric(Numerics::NoSuchChannel(channame));
			return CmdResult::FAILURE;
		}
		if (!target)
		{
			user->WriteNumeric(Numerics::NoSuchNick(username));
			return CmdResult::FAILURE;
		}

		if (!channel->HasUser(target))
		{
			user->WriteNotice(INSP_FORMAT("*** User {} is not on channel {}", target->nick, channel->name));
			return CmdResult::FAILURE;
		}

		if (target->IsModeSet(servprotectmode))
		{
			user->WriteNumeric(ERR_RESTRICTED, channame, "Only a service may remove a service from a channel.");
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
			ModeHandler::Rank ulevel = channel->GetPrefixValue(user);
			ModeHandler::Rank tlevel = channel->GetPrefixValue(target);
			if ((!IS_LOCAL(user)) || ((ulevel > VOICE_VALUE) && (ulevel >= tlevel) && ((protectedrank == 0) || (tlevel < protectedrank))))
			{
				// REMOVE will be sent to the target's server and it will reply with a PART (or do nothing if it doesn't understand the command)
				if (!IS_LOCAL(target))
				{
					CommandBase::Params p;
					p.push_back(channel->name);
					p.push_back(target->uuid);
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
				std::string reason = "Removed by " + user->nick + ": " + reasonparam;

				channel->WriteRemoteNotice(INSP_FORMAT("{} removed {} from the channel", user->nick, target->nick));
				target->WriteNotice("*** " + user->nick + " removed you from " + channel->name + " with the message: " + reasonparam);

				channel->PartUser(target, reason);
			}
			else
			{
				user->WriteNotice(INSP_FORMAT("*** You do not have access to /REMOVE {} from {}", target->nick, channel->name));
				return CmdResult::FAILURE;
			}
		}
		else
		{
			/* m_nokicks.so was loaded and +Q was set, block! */
			user->WriteNumeric(ERR_RESTRICTED, channel->name, INSP_FORMAT("Can't remove user {} from channel (+{} is set)",
				target->nick, servprotectmode->GetModeChar()));
			return CmdResult::FAILURE;
		}

		return CmdResult::SUCCESS;
	}
};

class ModuleRemove final
	: public Module
	, public ISupport::EventListener
{
private:
	CommandRemove cmd;

public:
	ModuleRemove()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /REMOVE command which allows channel operators to force part users from a channel.")
		, ISupport::EventListener(this)
		, cmd(this)
	{
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["REMOVE"];
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("remove");
		cmd.supportnokicks = tag->getBool("supportnokicks");
		cmd.protectedrank = tag->getNum<ModeHandler::Rank>("protectedrank", 50000, 1);
	}
};

MODULE_INIT(ModuleRemove)
