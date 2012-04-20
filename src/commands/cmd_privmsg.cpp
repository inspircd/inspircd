/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
#include "commands/cmd_privmsg.h"

extern "C" DllExport  Command* init_command(InspIRCd* Instance)
{
	return new CommandPrivmsg(Instance);
}

CmdResult CommandPrivmsg::Handle (const std::vector<std::string>& parameters, User *user)
{
	User *dest;
	Channel *chan;
	CUList except_list;

	user->idle_lastmsg = ServerInstance->Time();

	if (ServerInstance->Parser->LoopCall(user, this, parameters, 0))
		return CMD_SUCCESS;

	if (parameters[0][0] == '$')
	{
		if (!user->HasPrivPermission("users/mass-message"))
			return CMD_SUCCESS;

		int MOD_RESULT = 0;
		std::string temp = parameters[1];
		FOREACH_RESULT(I_OnUserPreMessage,OnUserPreMessage(user, (void*)parameters[0].c_str(), TYPE_SERVER, temp, 0, except_list));
		if (MOD_RESULT)
			return CMD_FAILURE;

		const char* text = temp.c_str();
		const char* servermask = (parameters[0].c_str()) + 1;

		FOREACH_MOD(I_OnText,OnText(user, (void*)parameters[0].c_str(), TYPE_SERVER, text, 0, except_list));
		if (InspIRCd::Match(ServerInstance->Config->ServerName, servermask, NULL))
		{
			user->SendAll("PRIVMSG", "%s", text);
		}
		FOREACH_MOD(I_OnUserMessage,OnUserMessage(user, (void*)parameters[0].c_str(), TYPE_SERVER, text, 0, except_list));
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

		except_list[user] = user->nick;

		if (chan)
		{
			if (IS_LOCAL(user) && chan->GetStatus(user) < STATUS_VOICE)
			{
				if (chan->IsModeSet('n') && !chan->HasUser(user))
				{
					user->WriteNumeric(404, "%s %s :Cannot send to channel (no external messages)", user->nick.c_str(), chan->name.c_str());
					return CMD_FAILURE;
				}

				if (chan->IsModeSet('m'))
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
			int MOD_RESULT = 0;

			std::string temp = parameters[1];
			FOREACH_RESULT(I_OnUserPreMessage,OnUserPreMessage(user,chan,TYPE_CHANNEL,temp,status,except_list));
			if (MOD_RESULT) {
				return CMD_FAILURE;
			}
			const char* text = temp.c_str();

			/* Check again, a module may have zapped the input string */
			if (temp.empty())
			{
				user->WriteNumeric(412, "%s :No text to send", user->nick.c_str());
				return CMD_FAILURE;
			}

			FOREACH_MOD(I_OnText,OnText(user,chan,TYPE_CHANNEL,text,status,except_list));

			if (status)
			{
				if (ServerInstance->Config->UndernetMsgPrefix)
				{
					chan->WriteAllExcept(user, false, status, except_list, "PRIVMSG %c%s :%c %s", status, chan->name.c_str(), status, text);
				}
				else
				{
					chan->WriteAllExcept(user, false, status, except_list, "PRIVMSG %c%s :%s", status, chan->name.c_str(), text);
				}
			}
			else
			{
				chan->WriteAllExcept(user, false, status, except_list, "PRIVMSG %s :%s", chan->name.c_str(), text);
			}

			FOREACH_MOD(I_OnUserMessage,OnUserMessage(user,chan,TYPE_CHANNEL,text,status,except_list));
		}
		else
		{
			/* no such nick/channel */
			user->WriteNumeric(401, "%s %s :No such nick/channel", user->nick.c_str(), target);
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
			if (dest && strcasecmp(dest->server, targetserver + 1))
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

	if (dest)
	{
		if (parameters[1].empty())
		{
			user->WriteNumeric(412, "%s :No text to send", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (IS_AWAY(dest))
		{
			/* auto respond with aweh msg */
			user->WriteNumeric(301, "%s %s :%s", user->nick.c_str(), dest->nick.c_str(), dest->awaymsg.c_str());
		}

		int MOD_RESULT = 0;

		std::string temp = parameters[1];
		FOREACH_RESULT(I_OnUserPreMessage,OnUserPreMessage(user, dest, TYPE_USER, temp, 0, except_list));
		if (MOD_RESULT) {
			return CMD_FAILURE;
		}
		const char* text = temp.c_str();

		FOREACH_MOD(I_OnText,OnText(user, dest, TYPE_USER, text, 0, except_list));

		if (IS_LOCAL(dest))
		{
			// direct write, same server
			user->WriteTo(dest, "PRIVMSG %s :%s", dest->nick.c_str(), text);
		}

		FOREACH_MOD(I_OnUserMessage,OnUserMessage(user, dest, TYPE_USER, text, 0, except_list));
	}
	else
	{
		/* no such nick/channel */
		user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[0].c_str());
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}
