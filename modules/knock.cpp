/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2018-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006, 2008 Craig Edwards <brain@inspircd.org>
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
#include "clientprotocolmsg.h"
#include "modules/invite.h"
#include "numerichelper.h"

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
enum KnockNotify
	: uint8_t
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
class CommandKnock final
	: public Command
{
private:
	SimpleChannelMode& noknockmode;
	ChanModeReference inviteonlymode;
	Invite::API inviteapi;

public:
	int notify;

	CommandKnock(Module* Creator, SimpleChannelMode& Noknockmode)
		: Command(Creator, "KNOCK", 2, 2)
		, noknockmode(Noknockmode)
		, inviteonlymode(Creator, "inviteonly")
		, inviteapi(Creator)
	{
		penalty = 5000;
		syntax = { "<channel> :<reason>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		auto* c = ServerInstance->Channels.Find(parameters[0]);
		if (!c)
		{
			user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
			return CmdResult::FAILURE;
		}

		if (c->HasUser(user))
		{
			user->WriteNumeric(ERR_KNOCKONCHAN, c->name, FMT::format("Can't KNOCK on {}, you are already on that channel.", c->name));
			return CmdResult::FAILURE;
		}

		if (c->IsModeSet(noknockmode))
		{
			user->WriteNumeric(ERR_CANNOTKNOCK, FMT::format("Can't KNOCK on {}, +{} is set.", c->name, noknockmode.GetModeChar()));
			return CmdResult::FAILURE;
		}

		if (!c->IsModeSet(inviteonlymode))
		{
			user->WriteNumeric(ERR_CHANOPEN, c->name, FMT::format("Can't KNOCK on {}, channel is not invite only so knocking is pointless!", c->name));
			return CmdResult::FAILURE;
		}

		// Work out who we should send the knock to.
		char status;
		switch (inviteapi->GetAnnounceState())
		{
			case Invite::ANNOUNCE_ALL:
			{
				status = 0;
				break;
			}

			case Invite::ANNOUNCE_DYNAMIC:
			{
				PrefixMode* mh = ServerInstance->Modes.FindNearestPrefixMode(HALFOP_VALUE);
				status = mh->GetPrefix() ? mh->GetPrefix() : '@';
				break;
			}

			default:
			{
				status = '@';
				break;
			}
		}

		if (notify & KN_SEND_NOTICE)
		{
			c->WriteNotice(FMT::format("User {} is KNOCKing on {} ({})", user->nick, c->name, parameters[1]), status);
			user->WriteNotice("KNOCKing on " + c->name);
		}

		if (notify & KN_SEND_NUMERIC)
		{
			Numeric::Numeric numeric(RPL_KNOCK);
			numeric.push(c->name).push(user->GetMask()).push("is KNOCKing: " + parameters[1]);

			ClientProtocol::Messages::Numeric numericmsg(numeric, c->name);
			c->Write(ServerInstance->GetRFCEvents().numeric, numericmsg, status);

			user->WriteNumeric(RPL_KNOCKDLVR, c->name, "KNOCKing on channel");
		}

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_BCAST;
	}
};

class ModuleKnock final
	: public Module
{
	SimpleChannelMode kn;
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
		const auto& tag = ServerInstance->Config->ConfValue("knock");
		cmd.notify = tag->getEnum("notify", KN_SEND_NOTICE, {
			{ "both",    KN_SEND_BOTH },
			{ "notice",  KN_SEND_NOTICE },
			{ "numeric", KN_SEND_NUMERIC },
		});
	}
};

MODULE_INIT(ModuleKnock)
