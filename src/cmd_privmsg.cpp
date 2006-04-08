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
#include "cmd_privmsg.h"

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;
extern int MODCOUNT;
extern ModuleList modules;
extern FactoryList factory;
extern time_t TIME;

void cmd_privmsg::Handle (char **parameters, int pcnt, userrec *user)
{
	userrec *dest;
	chanrec *chan;

	user->idle_lastmsg = TIME;
	
	if (ServerInstance->Parser->LoopCall(this,parameters,pcnt,user,0,pcnt-2,0))
		return;
	if ((parameters[0][0] == '$') && (*user->oper))
	{
		// notice to server mask
		char* servermask = parameters[0];
		servermask++;
		if (match(Config->ServerName,servermask))
		{
			ServerPrivmsgAll("%s",parameters[1]);
		}
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
		chan = FindChan(parameters[0]);
		if (chan)
		{
			if (IS_LOCAL(user))
			{
				if ((chan->modes[CM_NOEXTERNAL]) && (!chan->HasUser(user)))
				{
					WriteServ(user->fd,"404 %s %s :Cannot send to channel (no external messages)", user->nick, chan->name);
					return;
				}
				if ((chan->modes[CM_MODERATED]) && (cstatus(user,chan)<STATUS_VOICE))
				{
					WriteServ(user->fd,"404 %s %s :Cannot send to channel (+m)", user->nick, chan->name);
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
				WriteServ(user->fd,"412 %s No text to send", user->nick);
				return;
			}
			
			ChanExceptSender(chan, user, status, "PRIVMSG %s :%s", chan->name, parameters[1]);
			FOREACH_MOD(I_OnUserMessage,OnUserMessage(user,chan,TYPE_CHANNEL,parameters[1],status));
		}
		else
		{
			/* no such nick/channel */
			WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
		}
		return;
	}

	dest = Find(parameters[0]);
	if (dest)
	{
		if ((IS_LOCAL(user)) && (*dest->awaymsg))
		{
			/* auto respond with aweh msg */
			WriteServ(user->fd,"301 %s %s :%s",user->nick,dest->nick,dest->awaymsg);
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
			WriteTo(user, dest, "PRIVMSG %s :%s", dest->nick, parameters[1]);
		}

		FOREACH_MOD(I_OnUserMessage,OnUserMessage(user,dest,TYPE_USER,parameters[1],0));
	}
	else
	{
		/* no such nick/channel */
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
	}
}
