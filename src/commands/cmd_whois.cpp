/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/** Handle /WHOIS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandWhois : public Command
{
 public:
	/** Constructor for whois.
	 */
	CommandWhois ( Module* parent) : Command(parent,"WHOIS",1) { Penalty = 2; syntax = "<nick>{,<nick>}"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};


CmdResult CommandWhois::Handle (const std::vector<std::string>& parameters, User *user)
{
	User *dest;
	int userindex = 0;
	unsigned long idle = 0, signon = 0;

	if (ServerInstance->Parser->LoopCall(user, this, parameters, 0))
		return CMD_SUCCESS;


	/*
	 * If 2 paramters are specified (/whois nick nick), ignore the first one like spanningtree
	 * does, and use the second one, otherwise, use the only paramter. -- djGrrr
	 */
	if (parameters.size() > 1)
		userindex = 1;

	if (IS_LOCAL(user))
		dest = ServerInstance->FindNickOnly(parameters[userindex]);
	else
		dest = ServerInstance->FindNick(parameters[userindex]);

	if (dest)
	{
		/*
		 * Okay. Umpteenth attempt at doing this, so let's re-comment...
		 * For local users (/w localuser), we show idletime if hidewhois is disabled
		 * For local users (/w localuser localuser), we always show idletime, hence parameters.size() > 1 check.
		 * For remote users (/w remoteuser), we do NOT show idletime
		 * For remote users (/w remoteuser remoteuser), spanningtree will handle calling do_whois, so we can ignore this case.
		 * Thanks to djGrrr for not being impatient while I have a crap day coding. :p -- w00t
		 */
		if (IS_LOCAL(dest) && (ServerInstance->Config->HideWhoisServer.empty() || parameters.size() > 1))
		{
			idle = abs((long)((dest->idle_lastmsg)-ServerInstance->Time()));
			signon = dest->signon;
		}

		ServerInstance->DoWhois(user,dest,signon,idle,parameters[userindex].c_str());
	}
	else
	{
		/* no such nick/channel */
		user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), !parameters[userindex].empty() ? parameters[userindex].c_str() : "*");
		user->WriteNumeric(318, "%s %s :End of /WHOIS list.",user->nick.c_str(), !parameters[userindex].empty() ? parameters[userindex].c_str() : "*");
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}



COMMAND_INIT(CommandWhois)
