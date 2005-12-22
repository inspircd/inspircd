/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
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

/* $ModDesc: Gives /cban, aka C:lines. Think Q:lines, for channels. */

Server *Srv;

class CBan
{
 private:
	unsigned long expiry;
	std::string chname;
	std::string reason;

 public:
	CBan(std::string chname, std::string reason, unsigned long expiry)
	{

	}

	std::string GetName()
	{
		return chname;
	}

	std::string GetReason()
	{
		return reason;
	}

	unsigned long GetExpiry()
	{
		return expiry;
	}
};

class cmd_cban : public command_t
{
 public:
	cmd_cban () : command_t("CBAN", 'o', 1)
	{
		this->source = "m_cban.so";
	}

	void Handle(char **parameters, int pcnt, userrec *user)
	{
		/* syntax: CBAN #channel time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */

		if (pcnt == 1)
		{
			/* form: CBAN #channel removes a CBAN */
		}
		else if (pcnt >= 2)
		{
			/* full form to add a CBAN */
		}
	}
};

class ModuleCBan : public Module
{
	cmd_cban* mycommand;
	vector<CBan> cbans;

 public:
	ModuleCBan(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_cban();
		Srv->AddCommand(mycommand);
	}

	virtual int OnUserPreJoin (userrec *user, chanrec *chan, const char *cname)
	{
		/* check cbans in here, and apply as necessary. */

		std::string chname = cname;

		for (vector<CBan>::iterator iterate = cbans.begin(); iterate < cbans.end(); iterate++)
		{
			CBan *comp = (CBan *)*iterate; /* *mindsplode* */

			if (chname == comp->GetName())
			{
				/* matches CBAN */
				return 1;
			}
		}

		/* Allow the change. */
		return 0;
	}

	virtual ~ModuleCBan()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR);
	}
};


class ModuleCBanFactory : public ModuleFactory
{
 public:
	ModuleCBanFactory()
	{
	}
	
	~ModuleCBanFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleCBan(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleCBanFactory;
}

