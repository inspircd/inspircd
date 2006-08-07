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
#include "helperfuncs.h"
#include "wildcard.h"
#include "dns.h"

/* $ModDesc: Provides /tline command used to test who a mask matches */

static Server *Srv;
extern user_hash clientlist;
	 
class cmd_tline : public command_t
{
 public:
	cmd_tline () : command_t("TLINE", 'o', 1)
	{
		this->source = "m_tline.so";
		this->syntax = "<mask>";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		float n_counted = 0;
		float n_matched = 0;
		float n_match_host = 0;
		float n_match_ip = 0;

		for (user_hash::const_iterator u = clientlist.begin(); u != clientlist.end(); u++)
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
				sprintf(host, "%s@%s", u->second->ident, u->second->GetIPString());
				if (match(host, parameters[0], true))
				{
					n_matched++;
					n_match_ip++;
				}
			}
		}
		if (n_matched)
			WriteServ(user->fd, "NOTICE %s :*** TLINE: Counted %0.0f user(s). Matched '%s' against %0.0f user(s) (%0.2f%% of the userbase). %0.0f by hostname and %0.0f by IP address.",user->nick, n_counted, parameters[0], n_matched, (n_counted/n_matched)*100, n_match_host, n_match_ip);
		else
			WriteServ(user->fd, "NOTICE %s :*** TLINE: Counted %0.0f user(s). Matched '%s' against no user(s).", user->nick, n_counted, parameters[0]);
			
	}
};

class ModuleTLine : public Module
{
	cmd_tline* newcommand;
 public:
	ModuleTLine(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		newcommand = new cmd_tline();
		Srv->AddCommand(newcommand);
	}

	void Implements(char* List)
	{
	}

	virtual ~ModuleTLine()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleTLine(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleTLineFactory;
}

