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

#include <string>
#include "inspircd_config.h"
#include "inspircd.h"
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "message.h"
#include "wildcard.h"
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_privmsg.h"

extern InspIRCd* ServerInstance;
extern int MODCOUNT;
extern ModuleList modules;
extern FactoryList factory;
extern time_t TIME;

void cmd_privmsg::Handle (const char** parameters, int pcnt, userrec *user)
{
	userrec *dest;
	chanrec *chan;

	user->idle_lastmsg = TIME;
	
	if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 0))
		return;

	if ((parameters[0][0] == '$') && ((*user->oper) || (is_uline(user->server))))
	{
		int MOD_RESULT = 0;
		std::string temp = parameters[1];
		FOREACH_RESULT(I_OnUserPreMessage,OnUserPreMessage(user,(void*)parameters[0],TYPE_SERVER,temp,0));
		if (MOD_RESULT)
			return;
		parameters[1] = (char*)temp.c_str();
		// notice to server mask
		const char* servermask = parameters[0] + 1;
		if (match(ServerInstance->Config->ServerName,servermask))
		{
			ServerInstance->ServerPrivmsgAll("%s",parameters[1]);
		}
		FOREACH_MOD(I_OnUserMessage,OnUserMessage(user,(void*)parameters[0],TYPE_SERVER,parameters[1],0));
		return;
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
		if (chan)
		{
			if (IS_LOCAL(user))
			{
				if ((chan->modes[CM_NOEXTERNAL]) && (!chan->HasUser(user)))
				{
					user->WriteServ("404 %s %s :Cannot send to channel (no external messages)", user->nick, chan->name);
					return;
				}
				if ((chan->modes[CM_MODERATED]) && (chan->GetStatus(user) < STATUS_VOICE))
				{
					user->WriteServ("404 %s %s :Cannot send to channel (+m)", user->nick, chan->name);
					return;
				}
			}
			int MOD_RESULT = 0;

			std::string temp = parameters[1];
			FOREACH_RESULT(I_OnUserPreMessage,OnUserPreMessage(user,chan,TYPE_CHANNEL,temp,status));
			if (MOD_RESULT) {
				return;
			}
			parameters[1] = (char*)temp.c_str();

			if (temp == "")
			{
				user->WriteServ("412 %s No text to send", user->nick);
				return;
			}
			
			chan->WriteAllExceptSender(user, status, "PRIVMSG %s :%s", chan->name, parameters[1]);
			FOREACH_MOD(I_OnUserMessage,OnUserMessage(user,chan,TYPE_CHANNEL,parameters[1],status));
		}
		else
		{
			/* no such nick/channel */
			user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
		}
		return;
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
		FOREACH_RESULT(I_OnUserPreMessage,OnUserPreMessage(user,dest,TYPE_USER,temp,0));
		if (MOD_RESULT) {
			return;
		}
		parameters[1] = (char*)temp.c_str();

		if (dest->fd > -1)
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
	}
}
