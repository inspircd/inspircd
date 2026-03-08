/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2012, 2014-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "modules/invite.h"
#include "modules/ircv3.h"
#include "numerichelper.h"

class CommandUninvite final
	: public Command
{
private:
	Invite::API invapi;
	IRCv3::ReplyCapReference stdrplcap;

public:
	CommandUninvite(Module* Creator)
		: Command(Creator, "UNINVITE", 2)
		, invapi(Creator)
		, stdrplcap(Creator)
	{
		syntax = { "<nick> <channel>" };
		translation = { TranslateType::NICK, TranslateType::TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		User* u;
		if (user->IsLocal())
			u = ServerInstance->Users.FindNick(parameters[0], true);
		else
			u = ServerInstance->Users.Find(parameters[0]);

		auto* c = ServerInstance->Channels.Find(parameters[1]);

		if (!c || !u)
		{
			if (!c)
			{
				user->WriteNumeric(Numerics::NoSuchChannel(parameters[1]));
			}
			else
			{
				user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			}

			return CmdResult::FAILURE;
		}

		if (user->IsLocal())
		{
			if (c->GetPrefixValue(user) < HALFOP_VALUE)
			{
				user->WriteNumeric(Numerics::ChannelPrivilegesNeeded(c, HALFOP_VALUE, "remove an invite"));
				return CmdResult::FAILURE;
			}
		}

		/* Servers remember invites only for their local users, so act
		 * only if the target is local. Otherwise the command will be
		 * passed to the target users server.
		 */
		auto* lu = u->AsLocal();
		if (lu)
		{
			// XXX: The source of the numeric we send must be the server of the user doing the /UNINVITE,
			// so they don't see where the target user is connected to
			if (!invapi->Remove(lu, c))
			{
				Reply::Reply reply(Reply::FAIL, this, "NOT_INVITED");
				reply.SetSource(user->server);
				reply.PushParam(c->name, u->nick).PushParamFmt("{} has not been invited to {}.", u->nick, c->name);
				IRCv3::WriteReply(user, stdrplcap, reply);
				return CmdResult::FAILURE;
			}

			Reply::Reply reply(Reply::FAIL, this, "INVITE_REMOVED");
			reply.SetSource(user->server);
			reply.PushParam(c->name, u->nick).PushParamFmt("{} been uninvited from {}.", u->nick, c->name);
			IRCv3::WriteReply(user, stdrplcap, reply);

			IRCv3::WriteReply(Reply::NOTE, u, nullptr, this, "UNINVITED", c->name, user->nick,
				FMT::format("You were uninvited from {} by {}.", c->name, user->nick));

			c->WriteRemoteNotice(FMT::format("*** {} uninvited {}.", user->nick, u->nick));
		}

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};

class ModuleUninvite final
	: public Module
{
private:
	CommandUninvite cmd;

public:
	ModuleUninvite()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /UNINVITE command which allows users who have invited another user to a channel to withdraw their invite.")
		, cmd(this)
	{
	}
};

MODULE_INIT(ModuleUninvite)
