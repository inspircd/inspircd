/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2006 jamie
 *   Copyright (C) 2005 Robin Burchell <robin+git@viroteck.net>
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

class CommandSajoin final
	: public Command
{
private:
	UserModeReference servprotectmode;

public:
	CommandSajoin(Module* Creator)
		: Command(Creator, "SAJOIN", 1)
		, servprotectmode(Creator, "servprotect")
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "[<nick>] <channel>[,<channel>]+" };
		translation = { TR_NICK, TR_TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		const unsigned int channelindex = (parameters.size() > 1) ? 1 : 0;
		if (CommandParser::LoopCall(user, this, parameters, channelindex))
			return CmdResult::FAILURE;

		const std::string& channel = parameters[channelindex];
		const std::string& nickname = parameters.size() > 1 ? parameters[0] : user->nick;

		auto* dest = ServerInstance->Users.Find(nickname, true);
		if (dest)
		{
			if (user != dest && !user->HasPrivPermission("users/sajoin-others"))
			{
				user->WriteNotice("*** You are not allowed to /SAJOIN other users (the privilege users/sajoin-others is needed to /SAJOIN others).");
				return CmdResult::FAILURE;
			}

			if (dest->IsModeSet(servprotectmode))
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "Cannot use an SA command on a service");
				return CmdResult::FAILURE;
			}
			if (IS_LOCAL(user) && !ServerInstance->Channels.IsChannel(channel))
			{
				/* we didn't need to check this for each character ;) */
				user->WriteNumeric(ERR_BADCHANMASK, channel, "Invalid channel name");
				return CmdResult::FAILURE;
			}

			auto* chan = ServerInstance->Channels.Find(channel);
			if ((chan) && (chan->HasUser(dest)))
			{
				user->WriteRemoteNotice("*** " + dest->nick + " is already on " + channel);
				return CmdResult::FAILURE;
			}

			/* For local users, we call Channel::JoinUser which may create a channel and set its TS.
			 * For non-local users, we just return CmdResult::SUCCESS, knowing this will propagate it where it needs to be
			 * and then that server will handle the command.
			 */
			LocalUser* localuser = IS_LOCAL(dest);
			if (localuser)
			{
				Membership* memb = Channel::JoinUser(localuser, channel, true);
				if (memb)
				{
					ServerInstance->SNO.WriteGlobalSno('a', user->nick+" used SAJOIN to make "+dest->nick+" join "+channel);
					return CmdResult::SUCCESS;
				}
				else
				{
					user->WriteNotice("*** Could not join "+dest->nick+" to "+channel);
					return CmdResult::FAILURE;
				}
			}
			else
			{
				return CmdResult::SUCCESS;
			}
		}
		else
		{
			user->WriteNotice("*** No such nickname: '" + nickname + "'");
			return CmdResult::FAILURE;
		}
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};

class ModuleSajoin final
	: public Module
{
private:
	CommandSajoin cmd;

public:
	ModuleSajoin()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /SAJOIN command which allows server operators to force users to join one or more channels.")
		, cmd(this)
	{
	}
};

MODULE_INIT(ModuleSajoin)
