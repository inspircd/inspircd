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

/* $ModDesc: A module which logs all oper commands to the ircd log at default loglevel. */

class ModuleOperLog : public Module
{
 private:

 public:
	ModuleOperLog(InspIRCd* Me) : Module(Me)
	{

		Implementation eventlist[] = { I_OnPreCommand, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleOperLog()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}


	virtual ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return MOD_RES_PASSTHRU;

		if ((IS_OPER(user)) && (IS_LOCAL(user)) && (user->HasPermission(command)))
		{
			Command* thiscommand = ServerInstance->Parser->GetHandler(command);
			if ((thiscommand) && (thiscommand->flags_needed == 'o'))
				ServerInstance->Logs->Log("m_operlog",DEFAULT,"OPERLOG: [%s!%s@%s] %s %s",user->nick.c_str(), user->ident.c_str(), user->host.c_str(), command.c_str(), parameters.empty() ? "" : irc::stringjoiner(" ", parameters, 0, parameters.size() - 1).GetJoined().c_str());
		}

		return MOD_RES_PASSTHRU;
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" OPERLOG");
	}

};


MODULE_INIT(ModuleOperLog)
