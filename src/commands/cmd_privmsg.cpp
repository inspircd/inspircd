/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "wildcard.h"
#include "commands/cmd_privmsg.h"

extern "C" DllExport  Command* init_command(InspIRCd* Instance)
{
	return new CommandPrivmsg(Instance);
}

CmdResult CommandPrivmsg::Handle (const char** parameters, int pcnt, User *user)
{
	User *dest;
	Channel *chan;
	CUList except_list;

	user->idle_lastmsg = ServerInstance->Time();
	
	if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 0))
		return CMD_SUCCESS;

	if ((parameters[0][0] == '$') && (IS_OPER(user) || ServerInstance->ULine(user->server)))
	{
		int MOD_RESULT = 0;
		std::string temp = parameters[1];
		FOREACH_RESULT(I_OnUserPreMessage,OnUserPreMessage(user,(void*)parameters[0],TYPE_SERVER,temp,0,except_list));
		if (MOD_RESULT)
			return CMD_FAILURE;
		parameters[1] = temp.c_str();
		// notice to server mask
		const char* servermask = parameters[0] + 1;
		FOREACH_MOD(I_OnText,OnText(user,(void*)parameters[0],TYPE_SERVER,parameters[1],0,except_list));
		if (match(ServerInstance->Config->ServerName,servermask))
		{
			user->SendAll("PRIVMSG", "%s", parameters[1]);
		}
		FOREACH_MOD(I_OnUserMessage,OnUserMessage(user,(void*)parameters[0],TYPE_SERVER,parameters[1],0,except_list));
		return CMD_SUCCESS;
	}
	char status = 0;
	if ((*parameters[0] == '@') || (*parameters[0] == '%') || (*parameters[0] == '+'))
	{
		status = *parameters[0];
		parameters[0]++;
	}
	if (parameters[0][0] == '#')
	{
		chan = ServerInstance->FindChan(parameters[0]);

		except_list[user] = user->nick;

		if (chan)
		{
			if (IS_LOCAL(user))
			{
				if ((chan->IsModeSet('n')) && (!chan->HasUser(user)))
				{
					user->WriteServ("404 %s %s :Cannot send to channel (no external messages)", user->nick, chan->name);
					return CMD_FAILURE;
				}
				if ((chan->IsModeSet('m')) && (chan->GetStatus(user) < STATUS_VOICE))
				{
					user->WriteServ("404 %s %s :Cannot send to channel (+m)", user->nick, chan->name);
					return CMD_FAILURE;
				}
			}
			int MOD_RESULT = 0;

			std::string temp = parameters[1];
			FOREACH_RESULT(I_OnUserPreMessage,OnUserPreMessage(user,chan,TYPE_CHANNEL,temp,status,except_list));
			if (MOD_RESULT) {
				return CMD_FAILURE;
			}
			parameters[1] = temp.c_str();

			/* Check again, a module may have zapped the input string */
			if (temp.empty())
			{
				user->WriteServ("412 %s :No text to send", user->nick);
				return CMD_FAILURE;
			}

			FOREACH_MOD(I_OnText,OnText(user,chan,TYPE_CHANNEL,parameters[1],status,except_list));

			if (status)
			{
				if (ServerInstance->Config->UndernetMsgPrefix)
				{
					chan->WriteAllExcept(user, false, status, except_list, "PRIVMSG %c%s :%c %s", status, chan->name, status, parameters[1]);
				}
				else
				{
					chan->WriteAllExcept(user, false, status, except_list, "PRIVMSG %c%s :%s", status, chan->name, parameters[1]);
				}
			}
			else 
			{
				chan->WriteAllExcept(user, false, status, except_list, "PRIVMSG %s :%s", chan->name, parameters[1]);
			}

			FOREACH_MOD(I_OnUserMessage,OnUserMessage(user,chan,TYPE_CHANNEL,parameters[1],status,except_list));
		}
		else
		{
			/* no such nick/channel */
			user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
			return CMD_FAILURE;
		}
		return CMD_SUCCESS;
	}

	const char* destnick = parameters[0];

	if (IS_LOCAL(user))
	{
		const char* targetserver = strchr(destnick, '@');
	
		if (targetserver)
		{
			char nickonly[NICKMAX+1];
			strlcpy(nickonly, destnick, targetserver - destnick + 1);
			dest = ServerInstance->FindNickOnly(nickonly);
			if (dest && strcasecmp(dest->server, targetserver + 1))
			{
				/* Incorrect server for user */
				user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
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
		if (!*parameters[1])
		{
			user->WriteServ("412 %s :No text to send", user->nick);
			return CMD_FAILURE;
		}

		if (IS_AWAY(dest))
		{
			/* auto respond with aweh msg */
			user->WriteServ("301 %s %s :%s",user->nick,dest->nick,dest->awaymsg);
		}

		int MOD_RESULT = 0;
		
		std::string temp = parameters[1];
		FOREACH_RESULT(I_OnUserPreMessage,OnUserPreMessage(user,dest,TYPE_USER,temp,0,except_list));
		if (MOD_RESULT) {
			return CMD_FAILURE;
		}
		parameters[1] = (char*)temp.c_str();

		FOREACH_MOD(I_OnText,OnText(user,dest,TYPE_USER,parameters[1],0,except_list));

		if (IS_LOCAL(dest))
		{
			// direct write, same server
			user->WriteTo(dest, "PRIVMSG %s :%s", dest->nick, parameters[1]);
		}

		FOREACH_MOD(I_OnUserMessage,OnUserMessage(user,dest,TYPE_USER,parameters[1],0,except_list));
	}
	else
	{
		/* no such nick/channel */
		user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}

