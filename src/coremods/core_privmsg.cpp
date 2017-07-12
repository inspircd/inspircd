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

namespace
{
	const char* MessageTypeString[] = { "PRIVMSG", "NOTICE" };
}

class MessageCommandBase : public Command
{
	ChanModeReference moderatedmode;
	ChanModeReference noextmsgmode;

	/** Send a PRIVMSG or NOTICE message to all local users from the given user
	 * @param user User sending the message
	 * @param msg The message to send
	 * @param mt Type of the message (MSG_PRIVMSG or MSG_NOTICE)
	 */
	static void SendAll(User* user, const std::string& msg, MessageType mt);

 public:
	MessageCommandBase(Module* parent, MessageType mt)
		: Command(parent, MessageTypeString[mt], 2, 2)
		, moderatedmode(parent, "moderated")
		, noextmsgmode(parent, "noextmsg")
	{
		syntax = "<target>{,<target>} <message>";
	}

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult HandleMessage(const std::vector<std::string>& parameters, User* user, MessageType mt);

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (IS_LOCAL(user))
			// This is handled by the OnUserMessage hook to split the LoopCall pieces
			return ROUTE_LOCALONLY;
		else
			return ROUTE_MESSAGE(parameters[0]);
	}
};

void MessageCommandBase::SendAll(User* user, const std::string& msg, MessageType mt)
{
	const std::string message = ":" + user->GetFullHost() + " " + MessageTypeString[mt] + " $* :" + msg;
	const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
	for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); ++i)
	{
		if ((*i)->registered == REG_ALL)
			(*i)->Write(message);
	}
}

CmdResult MessageCommandBase::HandleMessage(const std::vector<std::string>& parameters, User* user, MessageType mt)
{
	User *dest;
	Channel *chan;
	CUList except_list;

	LocalUser* localuser = IS_LOCAL(user);
	if (localuser)
		localuser->idle_lastmsg = ServerInstance->Time();

	if (CommandParser::LoopCall(user, this, parameters, 0))
		return CMD_SUCCESS;

	if (parameters[0][0] == '$')
	{
		if (!user->HasPrivPermission("users/mass-message"))
			return CMD_SUCCESS;

		ModResult MOD_RESULT;
		std::string temp = parameters[1];
		FIRST_MOD_RESULT(OnUserPreMessage, MOD_RESULT, (user, (void*)parameters[0].c_str(), TYPE_SERVER, temp, 0, except_list, mt));
		if (MOD_RESULT == MOD_RES_DENY)
			return CMD_FAILURE;

		const char* text = temp.c_str();
		const char* servermask = (parameters[0].c_str()) + 1;

		FOREACH_MOD(OnText, (user, (void*)parameters[0].c_str(), TYPE_SERVER, text, 0, except_list));
		if (InspIRCd::Match(ServerInstance->Config->ServerName, servermask, NULL))
		{
			SendAll(user, text, mt);
		}
		FOREACH_MOD(OnUserMessage, (user, (void*)parameters[0].c_str(), TYPE_SERVER, text, 0, except_list, mt));
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

		except_list.insert(user);

		if (chan)
		{
			if (localuser && chan->GetPrefixValue(user) < VOICE_VALUE)
			{
				if (chan->IsModeSet(noextmsgmode) && !chan->HasUser(user))
				{
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (no external messages)");
					return CMD_FAILURE;
				}

				if (chan->IsModeSet(moderatedmode))
				{
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (+m)");
					return CMD_FAILURE;
				}

				if (ServerInstance->Config->RestrictBannedUsers)
				{
					if (chan->IsBanned(user))
					{
						user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (you're banned)");
						return CMD_FAILURE;
					}
				}
			}
			ModResult MOD_RESULT;

			std::string temp = parameters[1];
			FIRST_MOD_RESULT(OnUserPreMessage, MOD_RESULT, (user, chan, TYPE_CHANNEL, temp, status, except_list, mt));
			if (MOD_RESULT == MOD_RES_DENY)
				return CMD_FAILURE;

			const char* text = temp.c_str();

			/* Check again, a module may have zapped the input string */
			if (temp.empty())
			{
				user->WriteNumeric(ERR_NOTEXTTOSEND, "No text to send");
				return CMD_FAILURE;
			}

			FOREACH_MOD(OnText, (user,chan,TYPE_CHANNEL,text,status,except_list));

			if (status)
			{
				chan->WriteAllExcept(user, false, status, except_list, "%s %c%s :%s", MessageTypeString[mt], status, chan->name.c_str(), text);
			}
			else
			{
				chan->WriteAllExcept(user, false, status, except_list, "%s %s :%s", MessageTypeString[mt], chan->name.c_str(), text);
			}

			FOREACH_MOD(OnUserMessage, (user,chan, TYPE_CHANNEL, text, status, except_list, mt));
		}
		else
		{
			/* no such nick/channel */
			user->WriteNumeric(Numerics::NoSuchNick(target));
			return CMD_FAILURE;
		}
		return CMD_SUCCESS;
	}

	const char* destnick = parameters[0].c_str();

	if (localuser)
	{
		const char* targetserver = strchr(destnick, '@');

		if (targetserver)
		{
			std::string nickonly;

			nickonly.assign(destnick, 0, targetserver - destnick);
			dest = ServerInstance->FindNickOnly(nickonly);
			if (dest && strcasecmp(dest->server->GetName().c_str(), targetserver + 1))
			{
				/* Incorrect server for user */
				user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
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
			user->WriteNumeric(ERR_NOTEXTTOSEND, "No text to send");
			return CMD_FAILURE;
		}

		if ((dest->IsAway()) && (mt == MSG_PRIVMSG))
		{
			/* auto respond with aweh msg */
			user->WriteNumeric(RPL_AWAY, dest->nick, dest->awaymsg);
		}

		ModResult MOD_RESULT;

		std::string temp = parameters[1];
		FIRST_MOD_RESULT(OnUserPreMessage, MOD_RESULT, (user, dest, TYPE_USER, temp, 0, except_list, mt));
		if (MOD_RESULT == MOD_RES_DENY)
			return CMD_FAILURE;

		const char* text = temp.c_str();

		FOREACH_MOD(OnText, (user, dest, TYPE_USER, text, 0, except_list));

		if (IS_LOCAL(dest))
		{
			// direct write, same server
			dest->WriteFrom(user, "%s %s :%s", MessageTypeString[mt], dest->nick.c_str(), text);
		}

		FOREACH_MOD(OnUserMessage, (user, dest, TYPE_USER, text, 0, except_list, mt));
	}
	else
	{
		/* no such nick/channel */
		user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}

template<MessageType MT>
class CommandMessage : public MessageCommandBase
{
 public:
	CommandMessage(Module* parent)
		: MessageCommandBase(parent, MT)
	{
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		return HandleMessage(parameters, user, MT);
	}
};

class ModuleCoreMessage : public Module
{
	CommandMessage<MSG_PRIVMSG> CommandPrivmsg;
	CommandMessage<MSG_NOTICE> CommandNotice;

 public:
	ModuleCoreMessage()
		: CommandPrivmsg(this), CommandNotice(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("PRIVMSG, NOTICE", VF_CORE|VF_VENDOR);
	}
};

MODULE_INIT(ModuleCoreMessage)
