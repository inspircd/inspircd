/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2018, 2021-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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
#include "numerichelper.h"

#include "core_channel.h"

CommandKick::CommandKick(Module* parent)
	: Command(parent, "KICK", 2, 3)
{
	allow_empty_last_param = true;
	syntax = { "<channel> <nick>[,<nick>]+ [:<reason>]" };
}

CmdResult CommandKick::Handle(User* user, const Params& parameters)
{
	auto* c = ServerInstance->Channels.Find(parameters[0]);
	User* u;

	if (CommandParser::LoopCall(user, this, parameters, 1))
		return CmdResult::SUCCESS;

	if (IS_LOCAL(user))
		u = ServerInstance->Users.FindNick(parameters[1], true);
	else
		u = ServerInstance->Users.Find(parameters[1], true);

	if (!c)
	{
		user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
		return CmdResult::FAILURE;
	}
	if (!u)
	{
		user->WriteNumeric(Numerics::NoSuchNick(parameters[1]));
		return CmdResult::FAILURE;
	}

	Membership* srcmemb = nullptr;
	if (IS_LOCAL(user))
	{
		srcmemb = c->GetUser(user);
		if (!srcmemb)
		{
			user->WriteNumeric(ERR_NOTONCHANNEL, parameters[0], "You're not on that channel!");
			return CmdResult::FAILURE;
		}
	}

	const Channel::MemberMap::iterator victimiter = c->userlist.find(u);
	if (victimiter == c->userlist.end())
	{
		user->WriteNumeric(ERR_USERNOTINCHANNEL, u->nick, c->name, "They are not on that channel");
		return CmdResult::FAILURE;
	}
	Membership* const memb = victimiter->second;

	// KICKs coming from servers can carry a membership id
	if ((!IS_LOCAL(user)) && (parameters.size() > 3))
	{
		// If the current membership id is not equal to the one in the message then the user rejoined
		if (memb->id != Membership::IdFromString(parameters[2]))
		{
			ServerInstance->Logs.Debug(MODNAME, "Dropped KICK due to membership id mismatch: " + ConvToStr(memb->id) + " != " + parameters[2]);
			return CmdResult::FAILURE;
		}
	}

	const bool has_reason = (parameters.size() > 2);
	const std::string reason((has_reason ? parameters.back() : user->nick), 0, ServerInstance->Config->Limits.MaxKick);

	// Do the following checks only if the KICK is done by a local user;
	// each server enforces its own rules.
	if (srcmemb)
	{
		// Modules are allowed to explicitly allow or deny kicks done by local users
		ModResult res;
		FIRST_MOD_RESULT(OnUserPreKick, res, (user, memb, reason));
		if (res == MOD_RES_DENY)
			return CmdResult::FAILURE;

		if (res == MOD_RES_PASSTHRU)
		{
			ModeHandler::Rank them = srcmemb->GetRank();
			ModeHandler::Rank req = HALFOP_VALUE;
			for (const auto* mh : memb->modes)
			{
				if (mh->GetLevelRequired(true) > req)
					req = mh->GetLevelRequired(true);
			}

			if (them < req)
			{
				user->WriteNumeric(Numerics::ChannelPrivilegesNeeded(memb->chan, req, "kick a more privileged user"));
				return CmdResult::FAILURE;
			}
		}
	}

	c->KickUser(user, victimiter, reason);

	return CmdResult::SUCCESS;
}

RouteDescriptor CommandKick::GetRouting(User* user, const Params& parameters)
{
	return (IS_LOCAL(user) ? ROUTE_LOCALONLY : ROUTE_BROADCAST);
}
