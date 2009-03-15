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
#include "commands/cmd_notice.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandNotice(Instance);
}

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

		int MOD_RESULT = 0;
		std::string temp = parameters[1];
		FOREACH_RESULT(I_OnUserPreNotice,OnUserPreNotice(user, (void*)parameters[0].c_str(), TYPE_SERVER, temp, 0, exempt_list));
		if (MOD_RESULT)
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

		exempt_list[user] = user->nick;

		if (chan)
		{
			if (IS_LOCAL(user))
			{
				if ((chan->IsModeSet('n')) && (!chan->HasUser(user)))
				{
					user->WriteNumeric(404, "%s %s :Cannot send to channel (no external messages)", user->nick.c_str(), chan->name.c_str());
					return CMD_FAILURE;
				}
				if ((chan->IsModeSet('m')) && (chan->GetStatus(user) < STATUS_VOICE))
				{
					user->WriteNumeric(404, "%s %s :Cannot send to channel (+m)", user->nick.c_str(), chan->name.c_str());
					return CMD_FAILURE;
				}
			}
			int MOD_RESULT = 0;

			std::string temp = parameters[1];
			FOREACH_RESULT(I_OnUserPreNotice,OnUserPreNotice(user,chan,TYPE_CHANNEL,temp,status, exempt_list));
			if (MOD_RESULT) {
				return CMD_FAILURE;
			}
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

		int MOD_RESULT = 0;
		std::string temp = parameters[1];
		FOREACH_RESULT(I_OnUserPreNotice,OnUserPreNotice(user,dest,TYPE_USER,temp,0,exempt_list));
		if (MOD_RESULT) {
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
