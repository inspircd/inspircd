/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "wildcard.h"

/* $ModDesc: Provides the ability to abbreviate commands. */

class ModuleAbbreviation : public Module
{

 public:
	
	ModuleAbbreviation(InspIRCd* Me)
		: Module(Me)
	{
		Me->Modules->Attach(I_OnPreCommand, this);
		/* Must do this first */
		Me->Modules->SetPriority(this, I_OnPreCommand, PRIO_FIRST);
	}

	virtual Version GetVersion()
	{
		return Version(1,2,0,0,VF_VENDOR,API_VERSION);
	}

	virtual int OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
	{
		/* Command is already validated, has a length of 0, or last character is not a . */
		if (validated || command.empty() || *command.rbegin() != '.')
			return 0;

		/* Whack the . off the end */
		command.erase(command.end() - 1);

		ServerInstance->Logs->Log("m_abbreviation", DEBUG, "Abbreviated command: %s", command.c_str());

		size_t clen = command.length();
		for (Commandtable::iterator n = ServerInstance->Parser->cmdlist.begin(); n != ServerInstance->Parser->cmdlist.end(); ++n)
		{
			if (n->first.length() < clen)
				continue;

			ServerInstance->Logs->Log("m_abbreviation", DEBUG, "command=%s abbr=%s", command.c_str(), n->first.substr(0, clen).c_str());
			if (command == n->first.substr(0, clen))
			{
				/* Found the command */
				command = n->first;
				return false;
			}
		}

		command += '.';
		return false;
	}
};

MODULE_INIT(ModuleAbbreviation)
