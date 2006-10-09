/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "wildcard.h"
#include "inspircd.h"
#include "dns.h"

/* $ModDesc: Provides /tline command used to test who a mask matches */

/** Handle /TLINE
 */ 
class cmd_tline : public command_t
{
 public:
	cmd_tline (InspIRCd* Instance) : command_t(Instance,"TLINE", 'o', 1)
	{
		this->source = "m_tline.so";
		this->syntax = "<mask>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		float n_counted = 0;
		float n_matched = 0;
		float n_match_host = 0;
		float n_match_ip = 0;

		for (user_hash::const_iterator u = ServerInstance->clientlist.begin(); u != ServerInstance->clientlist.end(); u++)
		{
			n_counted++;
			if (match(u->second->GetFullRealHost(),parameters[0]))
			{
				n_matched++;
				n_match_host++;
			}
			else
			{
				char host[MAXBUF];
				snprintf(host, MAXBUF, "%s@%s", u->second->ident, u->second->GetIPString());
				if (match(host, parameters[0], true))
				{
					n_matched++;
					n_match_ip++;
				}
			}
		}
		if (n_matched)
			user->WriteServ( "NOTICE %s :*** TLINE: Counted %0.0f user(s). Matched '%s' against %0.0f user(s) (%0.2f%% of the userbase). %0.0f by hostname and %0.0f by IP address.",user->nick, n_counted, parameters[0], n_matched, (n_matched/n_counted)*100, n_match_host, n_match_ip);
		else
			user->WriteServ( "NOTICE %s :*** TLINE: Counted %0.0f user(s). Matched '%s' against no user(s).", user->nick, n_counted, parameters[0]);

		return CMD_FAILURE;
			
	}
};

class ModuleTLine : public Module
{
	cmd_tline* newcommand;
 public:
	ModuleTLine(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		newcommand = new cmd_tline(ServerInstance);
		ServerInstance->AddCommand(newcommand);
	}

	void Implements(char* List)
	{
	}

	virtual ~ModuleTLine()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR,API_VERSION);
	}
};


class ModuleTLineFactory : public ModuleFactory
{
 public:
	ModuleTLineFactory()
	{
	}
	
	~ModuleTLineFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleTLine(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleTLineFactory;
}

