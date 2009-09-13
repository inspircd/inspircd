/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/** Handle /PRIVMSG. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandPrivmsg : public Command
{
 public:
	/** Constructor for privmsg.
	 */
	CommandPrivmsg (InspIRCd* Instance, Module* parent) : Command(Instance,parent,"PRIVMSG",0,2) { syntax = "<target>{,<target>} <message>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

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

		ModResult MOD_RESULT;
		std::string temp = parameters[1];
		FIRST_MOD_RESULT(ServerInstance, OnUserPreMessage, MOD_RESULT, (user, (void*)parameters[0].c_str(), TYPE_SERVER, temp, 0, except_list));
		if (MOD_RESULT == MOD_RES_DENY)
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

		except_list.insert(user);

		if (chan)
		{
			if (IS_LOCAL(user) && chan->GetPrefixValue(user) < VOICE_VALUE)
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
			ModResult MOD_RESULT;

			std::string temp = parameters[1];
			FIRST_MOD_RESULT(ServerInstance, OnUserPreMessage, MOD_RESULT, (user,chan,TYPE_CHANNEL,temp,status,except_list));
			if (MOD_RESULT == MOD_RES_DENY)
				return CMD_FAILURE;

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

		ModResult MOD_RESULT;

		std::string temp = parameters[1];
		FIRST_MOD_RESULT(ServerInstance, OnUserPreMessage, MOD_RESULT, (user, dest, TYPE_USER, temp, 0, except_list));
		if (MOD_RESULT == MOD_RES_DENY)
			return CMD_FAILURE;

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

COMMAND_INIT(CommandPrivmsg)
