/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
/** Handle /NOTICE. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandNotice : public Command
{
 public:
	/** Constructor for notice.
	 */
	CommandNotice ( Module* parent) : Command(parent,"NOTICE",2,2) { syntax = "<target>{,<target>} <message>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (IS_LOCAL(user))
			// This is handled by the OnUserNotice hook to split the LoopCall pieces
			return ROUTE_LOCALONLY;
		else
			return ROUTE_MESSAGE(parameters[0]);
	}
};


CmdResult CommandNotice::Handle (const std::vector<std::string>& parameters, User *user)
{
	User *dest;
	Channel *chan;

	CUList exempt_list;

	user->idle_lastmsg = ServerInstance->Time();

	if (ServerInstance->Parser->LoopCall(user, this, parameters, 0))
		return CMD_SUCCESS;
	if (parameters[0][0] == '$')
	{
		if (!user->HasPrivPermission("users/mass-message"))
			return CMD_SUCCESS;

		ModResult MOD_RESULT;
		std::string temp = parameters[1];
		FIRST_MOD_RESULT(OnUserPreNotice, MOD_RESULT, (user, (void*)parameters[0].c_str(), TYPE_SERVER, temp, 0, exempt_list));
		if (MOD_RESULT == MOD_RES_DENY)
			return CMD_FAILURE;
		const char* text = temp.c_str();
		const char* servermask = (parameters[0].c_str()) + 1;

		FOREACH_MOD(I_OnText,OnText(user, (void*)parameters[0].c_str(), TYPE_SERVER, text, 0, exempt_list));
		if (InspIRCd::Match(ServerInstance->Config->ServerName,servermask, NULL))
		{
			user->SendAll("NOTICE", "%s", text);
		}
		FOREACH_MOD(I_OnUserNotice,OnUserNotice(user, (void*)parameters[0].c_str(), TYPE_SERVER, text, 0, exempt_list));
		return CMD_SUCCESS;
	}
	char status = 0;
	const char* target = parameters[0].c_str();

	if (ServerInstance->Modes->FindPrefix(*target))
	{
		status = *target;
		target++;
	}
	if (*target == '#')
	{
		chan = ServerInstance->FindChan(target);

		exempt_list.insert(user);

		if (chan)
		{
			if (IS_LOCAL(user))
			{
				if ((chan->IsModeSet('n')) && (!chan->HasUser(user)))
				{
					user->WriteNumeric(404, "%s %s :Cannot send to channel (no external messages)", user->nick.c_str(), chan->name.c_str());
					return CMD_FAILURE;
				}
				if ((chan->IsModeSet('m')) && (chan->GetPrefixValue(user) < VOICE_VALUE))
				{
					user->WriteNumeric(404, "%s %s :Cannot send to channel (+m)", user->nick.c_str(), chan->name.c_str());
					return CMD_FAILURE;
				}

				if (ServerInstance->Config->RestrictBannedUsers)
				{
					if (chan->IsBanned(user))
					{
						user->WriteNumeric(404, "%s %s :Cannot send to channel (you're banned)", user->nick.c_str(), chan->name.c_str());
						return CMD_FAILURE;
					}
				}
			}
			ModResult MOD_RESULT;

			std::string temp = parameters[1];
			FIRST_MOD_RESULT(OnUserPreNotice, MOD_RESULT, (user,chan,TYPE_CHANNEL,temp,status, exempt_list));
			if (MOD_RESULT == MOD_RES_DENY)
				return CMD_FAILURE;

			const char* text = temp.c_str();

			if (temp.empty())
			{
				user->WriteNumeric(412, "%s :No text to send", user->nick.c_str());
				return CMD_FAILURE;
			}

			FOREACH_MOD(I_OnText,OnText(user,chan,TYPE_CHANNEL,text,status,exempt_list));

			if (status)
			{
				if (ServerInstance->Config->UndernetMsgPrefix)
				{
					chan->WriteAllExcept(user, false, status, exempt_list, "NOTICE %c%s :%c %s", status, chan->name.c_str(), status, text);
				}
				else
				{
					chan->WriteAllExcept(user, false, status, exempt_list, "NOTICE %c%s :%s", status, chan->name.c_str(), text);
				}
			}
			else
			{
				chan->WriteAllExcept(user, false, status, exempt_list, "NOTICE %s :%s", chan->name.c_str(), text);
			}

			FOREACH_MOD(I_OnUserNotice,OnUserNotice(user,chan,TYPE_CHANNEL,text,status,exempt_list));
		}
		else
		{
			/* no such nick/channel */
			user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), target);
			return CMD_FAILURE;
		}
		return CMD_SUCCESS;
	}

	const char* destnick = parameters[0].c_str();

	if (IS_LOCAL(user))
	{
		const char* targetserver = strchr(destnick, '@');

		if (targetserver)
		{
			std::string nickonly;

			nickonly.assign(destnick, 0, targetserver - destnick);
			dest = ServerInstance->FindNickOnly(nickonly);
			if (dest && strcasecmp(dest->server.c_str(), targetserver + 1))
			{
				/* Incorrect server for user */
				user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}
		}
		else
			dest = ServerInstance->FindNickOnly(destnick);
	}
	else
		dest = ServerInstance->FindNick(destnick);

	if ((dest) && (dest->registered == REG_ALL))
	{
		if (parameters[1].empty())
		{
			user->WriteNumeric(412, "%s :No text to send", user->nick.c_str());
			return CMD_FAILURE;
		}

		ModResult MOD_RESULT;
		std::string temp = parameters[1];
		FIRST_MOD_RESULT(OnUserPreNotice, MOD_RESULT, (user,dest,TYPE_USER,temp,0,exempt_list));
		if (MOD_RESULT == MOD_RES_DENY) {
			return CMD_FAILURE;
		}
		const char* text = temp.c_str();

		FOREACH_MOD(I_OnText,OnText(user,dest,TYPE_USER,text,0,exempt_list));

		if (IS_LOCAL(dest))
		{
			// direct write, same server
			user->WriteTo(dest, "NOTICE %s :%s", dest->nick.c_str(), text);
		}

		FOREACH_MOD(I_OnUserNotice,OnUserNotice(user,dest,TYPE_USER,text,0,exempt_list));
	}
	else
	{
		/* no such nick/channel */
		user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[0].c_str());
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;

}

COMMAND_INIT(CommandNotice)
