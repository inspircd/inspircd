/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for USERIP command */

/** Handle /USERIP
 */
class cmd_userip : public command_t
{
 public:
	cmd_userip (InspIRCd* Instance) : command_t(Instance,"USERIP", 'o', 1)
	{
		this->source = "m_userip.so";
		syntax = "<nick>{,<nick>}";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		std::string retbuf = std::string("340 ") + user->nick + " :";

		for (int i = 0; i < pcnt; i++)
		{
			userrec *u = ServerInstance->FindNick(parameters[i]);
			if ((u) && (u->registered == REG_ALL))
			{
				retbuf = retbuf + u->nick + (IS_OPER(u) ? "*" : "") + "=+" + u->ident + "@" + u->GetIPString() + " ";
			}
		}

		user->WriteServ(retbuf);

		/* Dont send to the network */
		return CMD_FAILURE;
	}
};

class ModuleUserIP : public Module
{
	cmd_userip* mycommand;
 public:
	ModuleUserIP(InspIRCd* Me)
		: Module(Me)
	{
		
		mycommand = new cmd_userip(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		output = output + std::string(" USERIP");
	}
	
	virtual ~ModuleUserIP()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
	
};

MODULE_INIT(ModuleUserIP)

