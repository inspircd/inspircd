/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013, 2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

enum
{
	// From UnrealIRCd.
	ERR_CANNOTKNOCK = 480,

	// From ircd-ratbox.
	RPL_KNOCK = 710,
	RPL_KNOCKDLVR = 711,
	ERR_CHANOPEN = 713,
	ERR_KNOCKONCHAN = 714
};

// Actions which can be taken when a user knocks on a channel.
enum KnockNotify : uint8_t
{
	// Send a notice when a user knocks on a channel.
	KN_SEND_NOTICE = 1,

	// Send a numeric when a user knocks on a channel.
	KN_SEND_NUMERIC = 2,

	// Send a notice and a numeric when a user knocks on a channel.
	KN_SEND_BOTH = KN_SEND_NOTICE | KN_SEND_NUMERIC,
};

/** Handles the /KNOCK command
 */
class CommandKnock : public Command
{
	SimpleChannelModeHandler& noknockmode;
	ChanModeReference inviteonlymode;

 public:
	int notify;

	CommandKnock(Module* Creator, SimpleChannelModeHandler& Noknockmode)
		: Command(Creator,"KNOCK", 2, 2)
		, noknockmode(Noknockmode)
		, inviteonlymode(Creator, "inviteonly")
	{
		syntax = { "<channel> :<reason>" };
		Penalty = 5;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		Channel* c = ServerInstance->FindChan(parameters[0]);
		if (!c)
		{
			user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
			return CmdResult::FAILURE;
		}

		if (c->HasUser(user))
		{
			user->WriteNumeric(ERR_KNOCKONCHAN, c->name, InspIRCd::Format("Can't KNOCK on %s, you are already on that channel.", c->name.c_str()));
			return CmdResult::FAILURE;
		}

		if (c->IsModeSet(noknockmode))
		{
			user->WriteNumeric(ERR_CANNOTKNOCK, InspIRCd::Format("Can't KNOCK on %s, +K is set.", c->name.c_str()));
			return CmdResult::FAILURE;
		}

		if (!c->IsModeSet(inviteonlymode))
		{
			user->WriteNumeric(ERR_CHANOPEN, c->name, InspIRCd::Format("Can't KNOCK on %s, channel is not invite only so knocking is pointless!", c->name.c_str()));
			return CmdResult::FAILURE;
		}

		if (notify & KN_SEND_NOTICE)
		{
			c->WriteNotice(InspIRCd::Format("User %s is KNOCKing on %s (%s)", user->nick.c_str(), c->name.c_str(), parameters[1].c_str()));
			user->WriteNotice("KNOCKing on " + c->name);
		}

		if (notify & KN_SEND_NUMERIC)
		{
			Numeric::Numeric numeric(RPL_KNOCK);
			numeric.push(c->name).push(user->GetFullHost()).push("is KNOCKing: " + parameters[1]);

			ClientProtocol::Messages::Numeric numericmsg(numeric, c->name);
			c->Write(ServerInstance->GetRFCEvents().numeric, numericmsg);

			user->WriteNumeric(RPL_KNOCKDLVR, c->name, "KNOCKing on channel");
		}

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_BCAST;
	}
};

class ModuleKnock : public Module
{
	SimpleChannelModeHandler kn;
	CommandKnock cmd;

 public:
	ModuleKnock()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /KNOCK command which allows users to request access to an invite-only channel and channel mode K (noknock) which allows channels to disable usage of this command.")
		, kn(this, "noknock", 'K')
		, cmd(this, kn)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("knock");
		cmd.notify = tag->getEnum("notify", KN_SEND_NOTICE, {
			{ "both",    KN_SEND_BOTH },
			{ "notice",  KN_SEND_NOTICE },
			{ "numeric", KN_SEND_NUMERIC },
		});
	}
};

MODULE_INIT(ModuleKnock)
