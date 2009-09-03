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

/* $ModDesc: Provides /tline command used to test who a mask matches */

/** Handle /TLINE
 */
class CommandTline : public Command
{
 public:
	CommandTline (InspIRCd* Instance, Module* Creator) : Command(Instance, Creator,"TLINE", "o", 1)
	{
		this->syntax = "<mask>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		float n_counted = 0;
		float n_matched = 0;
		float n_match_host = 0;
		float n_match_ip = 0;

		for (user_hash::const_iterator u = ServerInstance->Users->clientlist->begin(); u != ServerInstance->Users->clientlist->end(); u++)
		{
			n_counted++;
			if (InspIRCd::Match(u->second->GetFullRealHost(),parameters[0]))
			{
				n_matched++;
				n_match_host++;
			}
			else
			{
				std::string host = std::string(u->second->ident) + "@" + u->second->GetIPString();
				if (InspIRCd::MatchCIDR(host, parameters[0]))
				{
					n_matched++;
					n_match_ip++;
				}
			}
		}
		if (n_matched)
			user->WriteServ( "NOTICE %s :*** TLINE: Counted %0.0f user(s). Matched '%s' against %0.0f user(s) (%0.2f%% of the userbase). %0.0f by hostname and %0.0f by IP address.",user->nick.c_str(), n_counted, parameters[0].c_str(), n_matched, (n_matched/n_counted)*100, n_match_host, n_match_ip);
		else
			user->WriteServ( "NOTICE %s :*** TLINE: Counted %0.0f user(s). Matched '%s' against no user(s).", user->nick.c_str(), n_counted, parameters[0].c_str());

		return CMD_SUCCESS;
	}
};

class ModuleTLine : public Module
{
	CommandTline cmd;
 public:
	ModuleTLine(InspIRCd* Me)
		: Module(Me), cmd(Me, this)
	{
		ServerInstance->AddCommand(&cmd);
	}


	virtual ~ModuleTLine()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleTLine)

