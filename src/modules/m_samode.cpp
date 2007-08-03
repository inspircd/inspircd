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

/* $ModDesc: Provides more advanced UnrealIRCd SAMODE command */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/** Handle /SAMODE
 */
class cmd_samode : public command_t
{
 public:
	cmd_samode (InspIRCd* Instance) : command_t(Instance,"SAMODE", 'o', 2)
	{
		this->source = "m_samode.so";
		syntax = "<target> <modes> {<mode-parameters>}";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		/*
		 * Handles an SAMODE request. Notifies all +s users.
	 	 */

		userrec* n = new userrec(ServerInstance);
		n->SetFd(FD_MAGIC_NUMBER);
		ServerInstance->SendMode(parameters,pcnt,n);
		delete n;

		if (ServerInstance->Modes->GetLastParse().length())
		{
			ServerInstance->WriteOpers("*** " + std::string(user->nick) + " used SAMODE: " + ServerInstance->Modes->GetLastParse());

			std::deque<std::string> n;
			irc::spacesepstream spaced(ServerInstance->Modes->GetLastParse());
			std::string one = "*";
			while ((one = spaced.GetToken()) != "")
				n.push_back(one);

			Event rmode((char *)&n, NULL, "send_mode");
			rmode.Send(ServerInstance);

			n.clear();
			n.push_back(std::string(user->nick) + " used SAMODE: " + ServerInstance->Modes->GetLastParse());
			Event rmode2((char *)&n, NULL, "send_opers");
			rmode2.Send(ServerInstance);

			/* XXX: Yes, this is right. We dont want to propogate the
			 * actual SAMODE command, just the MODE command generated
			 * by the send_mode
			 */
			return CMD_LOCALONLY;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Invalid SAMODE sequence.", user->nick);
		}

		return CMD_FAILURE;
	}
};

class ModuleSaMode : public Module
{
	cmd_samode*	mycommand;
 public:
	ModuleSaMode(InspIRCd* Me)
		: Module(Me)
	{
		
		mycommand = new cmd_samode(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleSaMode()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleSaMode)
