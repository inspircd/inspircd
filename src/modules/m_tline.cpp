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

/* $ModDesc: Provides /tline command used to test who a mask matches */

/** Handle /TLINE
 */
class CommandTline : public Command
{
 public:
	CommandTline(Module* Creator) : Command(Creator,"TLINE", 1)
	{
		flags_needed = 'o'; this->syntax = "<mask>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		unsigned long n_counted = 0;
		unsigned long n_matched = 0;
		unsigned long n_match_host = 0;
		unsigned long n_match_ip = 0;

		for (user_hash::const_iterator u = ServerInstance->Users->clientlist->begin(); u != ServerInstance->Users->clientlist->end(); ++u)
		{
			++n_counted;
			if (InspIRCd::Match(u->second->GetFullRealHost(),parameters[0]))
			{
				++n_matched;
				++n_match_host;
			}
			else
			{
				std::string host = std::string(u->second->ident) + "@" + u->second->GetIPString();
				if (InspIRCd::MatchCIDR(host, parameters[0]))
				{
					++n_matched;
					++n_match_ip;
				}
			}
		}
		if (n_matched)
			user->WriteServ( "NOTICE %s :*** TLINE: Counted %lu user(s). Matched '%s' against %lu user(s) (%0.2f%% of the userbase). %lu by hostname and %lu by IP address.",user->nick.c_str(), n_counted, parameters[0].c_str(), n_matched, 100.0f*n_matched/n_counted, n_match_host, n_match_ip);
		else
			user->WriteServ( "NOTICE %s :*** TLINE: Counted %lu user(s). Matched '%s' against no user(s).", user->nick.c_str(), n_counted, parameters[0].c_str());

		return CMD_SUCCESS;
	}
};

class ModuleTLine : public Module
{
	CommandTline cmd;
 public:
	ModuleTLine() : cmd(this) {}

	void init()
	{
		ServerInstance->AddCommand(&cmd);
	}

	Version GetVersion()
	{
		return Version("Provides /tline command used to test who a mask matches", VF_VENDOR);
	}
};

MODULE_INIT(ModuleTLine)

