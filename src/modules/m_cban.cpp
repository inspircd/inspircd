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

extern time_t TIME;

class CBan
{
 private:
	unsigned long expiry;
	std::string chname;
	std::string reason;

 public:
	CBan(std::string cn, std::string rs, unsigned long ex)
	{
		chname = cn;
		reason = rs;
		expiry = ex;
	}

	std::string GetName()
	{
		return this->chname;
	}

	std::string GetReason()
	{
		return this->reason;
	}

	unsigned long GetExpiry()
	{
		return this->expiry;
	}
};

/* cbans is declared here, as our type is right above. Don't try move it. */
vector<CBan> cbans;

class cmd_cban : public command_t
{
 private:
	Server *Srv;

 public:
	cmd_cban (Server* Me) : command_t("CBAN", 'o', 1)
	{
		this->source = "m_cban.so";
		this->Srv = Me;
	}

	void Handle(char **parameters, int pcnt, userrec *user)
	{
		/* syntax: CBAN #channel time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */

		std::string chname;
		std::string reason;
		unsigned long expiry;

		if (pcnt == 1)
		{
			/* form: CBAN #channel removes a CBAN */
			for (vector<CBan>::iterator myiter; myiter < cbans.end(); myiter++)
			{
				if (parameters[0] == (*myiter).GetName())
				{
					cbans.erase(myiter);
					break;
				}
			}
		}
		else if (pcnt >= 2)
		{
			/* full form to add a CBAN */
			/* XXX - checking on chnames */
			chname = parameters[0];
			expiry = TIME + Srv->CalcDuration(parameters[1]);
			reason = parameters[2];

			CBan meow(chname, reason, expiry);
			cbans.push_back(meow);
		}
	}
};

class ModuleCBan : public Module
{
	cmd_cban* mycommand;
	Server* Srv;

 public:
	ModuleCBan(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_cban(Srv);
		Srv->AddCommand(mycommand);
	}

	virtual int OnUserPreJoin(userrec *user, chanrec *chan, const char *cname)
	{
		/* check cbans in here, and apply as necessary. */
		log(DEBUG,"In OnUserPreJoin cbans.size() == %d",cbans.size());

		std::string chname = cname;

		for (unsigned int a = 0; a < cbans.size(); a++)
		{
			log(DEBUG,"m_cban: DEBUG: checking %s against %s in OnPreUserJoin()", chname.c_str(), cbans[a].GetName().c_str());
			if (chname == cbans[a].GetName())
			{
				/* matches CBAN */
				WriteOpers("DENY join");
				return 1;
			}
		}

		log(DEBUG,"DONE checking, allowed");

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

