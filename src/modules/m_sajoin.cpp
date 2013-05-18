/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2005, 2007 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Provides command SAJOIN to allow opers to force-join users to channels */

/** Handle /SAJOIN
 */
class CommandSajoin : public Command
{
 public:
	CommandSajoin(Module* Creator) : Command(Creator,"SAJOIN", 2)
	{
		allow_empty_last_param = false;
		flags_needed = 'o'; Penalty = 0; syntax = "<nick> <channel>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if ((dest) && (dest->registered == REG_ALL))
		{
			if (ServerInstance->ULine(dest->server))
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Cannot use an SA command on a u-lined client",user->nick.c_str());
				return CMD_FAILURE;
			}
			if (IS_LOCAL(user) && !ServerInstance->IsChannel(parameters[1]))
			{
				/* we didn't need to check this for each character ;) */
				user->WriteNotice("*** Invalid characters in channel name or name too long");
				return CMD_FAILURE;
			}

			/* For local users, we call Channel::JoinUser which may create a channel and set its TS.
			 * For non-local users, we just return CMD_SUCCESS, knowing this will propagate it where it needs to be
			 * and then that server will handle the command.
			 */
			LocalUser* localuser = IS_LOCAL(dest);
			if (localuser)
			{
				Channel* n = Channel::JoinUser(localuser, parameters[1], true);
				if (n)
				{
					if (n->HasUser(dest))
					{
						ServerInstance->SNO->WriteToSnoMask('a', user->nick+" used SAJOIN to make "+dest->nick+" join "+parameters[1]);
						return CMD_SUCCESS;
					}
					else
					{
						user->WriteNotice("*** Could not join "+dest->nick+" to "+parameters[1]+" (User is probably banned, or blocking modes)");
						return CMD_FAILURE;
					}
				}
				else
				{
					user->WriteNotice("*** Could not join "+dest->nick+" to "+parameters[1]);
					return CMD_FAILURE;
				}
			}
			else
			{
				ServerInstance->SNO->WriteToSnoMask('a', user->nick+" sent remote SAJOIN to make "+dest->nick+" join "+parameters[1]);
				return CMD_SUCCESS;
			}
		}
		else
		{
			user->WriteNotice("*** No such nickname "+parameters[0]);
			return CMD_FAILURE;
		}
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
			return ROUTE_OPT_UCAST(dest->server);
		return ROUTE_LOCALONLY;
	}
};

class ModuleSajoin : public Module
{
	CommandSajoin cmd;
 public:
	ModuleSajoin()
		: cmd(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->Modules->AddService(cmd);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides command SAJOIN to allow opers to force-join users to channels", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleSajoin)
