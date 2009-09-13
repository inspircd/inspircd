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
#include "xline.h"

/** Handle /QLINE.  */
class CommandQline : public Command
{
 public:
	/** Constructor for qline.
	 */
	CommandQline ( Module* parent) : Command(parent,"QLINE",1,3) { flags_needed = 'o'; Penalty = 0; syntax = "<nick> [<duration> :<reason>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};


CmdResult CommandQline::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() >= 3)
	{
		if (ServerInstance->NickMatchesEveryone(parameters[0],user))
			return CMD_FAILURE;

		if (parameters[0].find('@') != std::string::npos || parameters[0].find('!') != std::string::npos || parameters[0].find('.') != std::string::npos)
		{
			user->WriteServ("NOTICE %s :*** A Q-Line only bans a nick pattern, not a nick!user@host pattern.",user->nick.c_str());
			return CMD_FAILURE;
		}

		long duration = ServerInstance->Duration(parameters[1].c_str());
		QLine* ql = new QLine(ServerInstance, ServerInstance->Time(), duration, user->nick.c_str(), parameters[2].c_str(), parameters[0].c_str());
		if (ServerInstance->XLines->AddLine(ql,user))
		{
			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent Q-line for %s: %s",user->nick.c_str(), parameters[0].c_str(), parameters[2].c_str());
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed Q-line for %s, expires on %s: %s",user->nick.c_str(),parameters[0].c_str(),
					  ServerInstance->TimeString(c_requires_crap).c_str(), parameters[2].c_str());
			}
			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete ql;
			user->WriteServ("NOTICE %s :*** Q-Line for %s already exists",user->nick.c_str(),parameters[0].c_str());
		}
	}
	else
	{
		if (ServerInstance->XLines->DelLine(parameters[0].c_str(), "Q", user))
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed Q-line on %s.",user->nick.c_str(),parameters[0].c_str());
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Q-Line %s not found in list, try /stats q.",user->nick.c_str(),parameters[0].c_str());
			return CMD_FAILURE;
		}
	}

	return CMD_SUCCESS;
}


COMMAND_INIT(CommandQline)
