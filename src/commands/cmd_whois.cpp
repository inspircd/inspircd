/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "command_parse.h"

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
		 * For local whois of local users (/w localuser), we show idletime if hidewhois is disabled
		 * For local whois of remote users (/w remoteuser), we do not show idletime
		 * For remote whois (/w user user), we always show idletime
		 */
		bool showIdle = (parameters.size() > 1);
		if (IS_LOCAL(dest) && ServerInstance->Config->HideWhoisServer.empty())
			showIdle = true;

		if (showIdle)
			dest->DoWhois(user);
		else
			ServerInstance->DoWhois(user,dest,0,0);
	}
	else
	{
		/* no such nick/channel */
		user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), !parameters[userindex].empty() ? parameters[userindex].c_str() : "*");
		user->WriteNumeric(318, "%s %s :End of /WHOIS list.",user->nick.c_str(), parameters[userindex].empty() ? parameters[userindex].c_str() : "*");
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}



COMMAND_INIT(CommandWhois)
