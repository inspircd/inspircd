/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "wildcard.h"
#include "commands/cmd_privmsg.h"



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_privmsg(Instance);
}

CmdResult cmd_privmsg::Handle (const char** parameters, int pcnt, userrec *user)
{
	userrec *dest;
	chanrec *chan;

	CUList except_list;

	user->idle_lastmsg = ServerInstance->Time();
	
	if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 0))
		return CMD_SUCCESS;

	if ((parameters[0][0] == '$') && ((*user->oper) || (ServerInstance->ULine(user->server))))
	{
		int MOD_RESULT = 0;
		std::string temp = parameters[1];
		FOREACH_RESULT(I_OnUserPreMessage,OnUserPreMessage(user,(void*)parameters[0],TYPE_SERVER,temp,0,except_list));
		if (MOD_RESULT)
			return CMD_FAILURE;
		parameters[1] = (char*)temp.c_str();
		// notice to server mask
		const char* servermask = parameters[0] + 1;
		if (match(ServerInstance->Config->ServerName,servermask))
		{
			ServerInstance->ServerPrivmsgAll("%s",parameters[1]);
		}
		FOREACH_MOD(I_OnUserMessage,OnUserMessage(user,(void*)parameters[0],TYPE_SERVER,parameters[1],0));
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

		except_list[user] = user;

		if (chan)
		{
			if (IS_LOCAL(user))
			{
				if ((chan->modes[CM_NOEXTERNAL]) && (!chan->HasUser(user)))
				{
					user->WriteServ("404 %s %s :Cannot send to channel (no external messages)", user->nick, chan->name);
					return CMD_FAILURE;
				}
				if ((chan->modes[CM_MODERATED]) && (chan->GetStatus(user) < STATUS_VOICE))
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

			if (temp == "")
			{
				user->WriteServ("412 %s No text to send", user->nick);
				return CMD_FAILURE;
			}

			chan->WriteAllExcept(user, false, status, except_list, "PRIVMSG %s :%s", chan->name, parameters[1]);
			FOREACH_MOD(I_OnUserMessage,OnUserMessage(user,chan,TYPE_CHANNEL,parameters[1],status));
		}
		else
		{
			/* no such nick/channel */
			user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
			return CMD_FAILURE;
		}
		return CMD_SUCCESS;
	}

	dest = ServerInstance->FindNick(parameters[0]);
	if (dest)
	{
		if ((IS_LOCAL(user)) && (*dest->awaymsg))
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

		if (IS_LOCAL(dest))
		{
			// direct write, same server
			user->WriteTo(dest, "PRIVMSG %s :%s", dest->nick, parameters[1]);
		}

		FOREACH_MOD(I_OnUserMessage,OnUserMessage(user,dest,TYPE_USER,parameters[1],0));
	}
	else
	{
		/* no such nick/channel */
		user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}

