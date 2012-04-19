/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides support for USERIP command */

/** Handle /USERIP
 */
class CommandUserip : public Command
{
 public:
	CommandUserip (InspIRCd* Instance) : Command(Instance,"USERIP", "o", 1)
	{
		this->source = "m_userip.so";
		syntax = "<nick>{,<nick>}";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		std::string retbuf = std::string("340 ") + user->nick + " :";
		int nicks = 0;

		for (int i = 0; i < (int)parameters.size(); i++)
		{
			User *u = ServerInstance->FindNick(parameters[i]);
			if ((u) && (u->registered == REG_ALL))
			{
				retbuf = retbuf + u->nick + (IS_OPER(u) ? "*" : "") + "=";
				if (IS_AWAY(u))
					retbuf += "-";
				else
					retbuf += "+";
				retbuf += u->ident + "@" + u->GetIPString() + " ";
				nicks++;
			}
		}

		if (nicks != 0)
			user->WriteServ(retbuf);

		/* Dont send to the network */
		return CMD_LOCALONLY;
	}
};

class ModuleUserIP : public Module
{
	CommandUserip* mycommand;
 public:
	ModuleUserIP(InspIRCd* Me)
		: Module(Me)
	{

		mycommand = new CommandUserip(ServerInstance);
		ServerInstance->AddCommand(mycommand);
		Implementation eventlist[] = { I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 1);
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
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}

};

MODULE_INIT(ModuleUserIP)

