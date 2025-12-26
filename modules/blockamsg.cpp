/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
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
#include "extension.h"
#include "xline.h"

enum BlockAction
{
	// Kill the user with the reason "Global message (/amsg or /ame) detected".
	IBLOCK_KILL,

	// As above, but send an oper notice as well. This is the default.
	IBLOCK_KILLOPERS,

	//  Send a notice to the user informing them of what happened.
	IBLOCK_NOTICE,

	// Send a notice to the user informing them and send an oper notice.
	IBLOCK_NOTICEOPERS,

	// Generate no output, silently drop messages.
	IBLOCK_SILENT,

	// ZLine the user with the reason "Global message (/amsg or /ame) detected".
	IBLOCK_ZLINE,

	// As above, but send an oper notice as well. This is the default.
	IBLOCK_ZLINEOPERS,
};

class BlockedMessage final
{
public:
	std::string message;
	std::string target;
	time_t sent;

	BlockedMessage(const std::string& msg, const std::string& tgt, time_t when)
		: message(msg)
		, target(tgt)
		, sent(when)
	{
	}
};

class ModuleBlockAmsg final
	: public Module
{
	unsigned long ForgetDelay;
	BlockAction action;
	unsigned long duration;
	SimpleExtItem<BlockedMessage> blockamsg;

public:
	ModuleBlockAmsg()
		: Module(VF_VENDOR, "Blocks mass messages sent using the /AME and /AMSG commands that exist in clients such as mIRC and HexChat.")
		, blockamsg(this, "blockamsg", ExtensionType::USER)
	{
	}
	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("blockamsg");
		action = tag->getEnum("action", IBLOCK_KILLOPERS, {
			{ "kill",        IBLOCK_KILL },
			{ "killopers",   IBLOCK_KILLOPERS },
			{ "notice",      IBLOCK_NOTICE },
			{ "noticeopers", IBLOCK_NOTICEOPERS },
			{ "silent",      IBLOCK_SILENT },
			{ "zline",       IBLOCK_ZLINE },
			{ "zlineopers",  IBLOCK_ZLINEOPERS },
		});
		duration = tag->getNum<unsigned long>("duration", 15*60, 1);

		ForgetDelay = tag->getDuration("delay", 3);
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		// Don't do anything with partially connected users
		if (!user->IsFullyConnected())
			return MOD_RES_PASSTHRU;

		if ((validated) && (parameters.size() >= 2) && ((command == "PRIVMSG") || (command == "NOTICE")))
		{
			if (user->HasPrivPermission("servers/ignore-blockamsg"))
				return MOD_RES_PASSTHRU;

			// parameters[0] is the target list, count how many channels are there
			unsigned int targets = 0;
			// Is the first target a channel?
			if (ServerInstance->Channels.IsPrefix(parameters[0][0]))
				targets = 1;

			for (const char* c = parameters[0].c_str(); *c; c++)
			{
				if (*c == ',' && ServerInstance->Channels.IsPrefix(*(c + 1)))
					targets++;
			}

			/* targets should now contain the number of channel targets the msg/notice was pointed at.
			 * If the msg/notice was a PM there should be no channel targets and 'targets' should = 0.
			 * We don't want to block PMs so...
			 */
			if (targets == 0)
				return MOD_RES_PASSTHRU;

			// Check that this message wasn't already sent within a few seconds.
			BlockedMessage* m = blockamsg.Get(user);

			// If the message is identical and within the time.
			// We check the target is *not* identical, that'd straying into the realms of flood control. Which isn't what we're doing...
			// OR
			// The number of target channels is equal to the number of channels the sender is on..a little suspicious.
			// Check it's more than 1 too, or else users on one channel would have fun.
			if ((m && (m->message == parameters[1]) &&
					(!irc::equals(m->target, parameters[0])) &&
					ForgetDelay &&
					(m->sent >= ServerInstance->Time()-(time_t)ForgetDelay)) ||
				((targets > 1) && (targets == user->chans.size())))
			{
				// Block it...
				if (action == IBLOCK_KILLOPERS || action == IBLOCK_NOTICEOPERS || action == IBLOCK_ZLINEOPERS)
				{
					ServerInstance->SNO.WriteToSnoMask('a', "User {} ({}) had an /amsg or /ame blocked",
						user->GetRealMask(), user->GetAddress());
				}
				if (action == IBLOCK_KILL || action == IBLOCK_KILLOPERS)
					ServerInstance->Users.QuitUser(user, "Attempted to global message (/amsg or /ame)");
				else if (action == IBLOCK_NOTICE || action == IBLOCK_NOTICEOPERS)
					user->WriteNotice("Global message (/amsg or /ame) blocked");
				else if (action == IBLOCK_ZLINE || action == IBLOCK_ZLINEOPERS)
				{
					auto* zl = new ZLine(ServerInstance->Time(), duration, MODNAME "@" + ServerInstance->Config->ServerName,
						"Attempted to global message (/amsg or /ame)", user->GetAddress());
					if (!ServerInstance->XLines->AddLine(zl, nullptr))
					{
						ServerInstance->Users.QuitUser(user, zl->reason);
						delete zl;
					}
				}
			}

			if (m)
			{
				// If there's already a BlockedMessage allocated, use it.
				m->message = parameters[1];
				m->target = parameters[0];
				m->sent = ServerInstance->Time();
			}
			else
			{
				blockamsg.SetFwd(user, parameters[1], parameters[0], ServerInstance->Time());
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleBlockAmsg)
