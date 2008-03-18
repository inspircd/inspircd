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
#include "m_cap.h"

/* $ModDesc: Provides a pointless /dalinfo command, demo module */

/*
 *          std::string type;
 *                   std::vector<std::string> parameters;
 *                            User* user;
 *                                     Module* creator;
 */

/** Handle /CAP
 */
class CommandCAP : public Command
{
	Module* Creator;
 public:
	/* Command 'dalinfo', takes no parameters and needs no special modes */
	CommandCAP (InspIRCd* Instance, Module* mod) : Command(Instance,"CAP", 0, 1, true), Creator(mod)
	{
		this->source = "m_cap.so";
	}

	CmdResult Handle (const char* const* parameters, int pcnt, User *user)
	{
		irc::string subcommand = parameters[0];
		if (subcommand == "REQ")
		{
			CapData Data;
			Data.type = parameters[1];
			Data.user = user;
			Data.creator = this->Creator;
			Data.parameter = (pcnt > 1 ? parameters[1] : "");

			user->Extend("CAP_REGHOLD");
			Event event((char*) &Data, (Module*)this->Creator, "cap_req");
			event.Send(this->ServerInstance);
		}
		else if (subcommand == "END")
		{
			user->Shrink("CAP_REGHOLD");
		}
		else if (subcommand == "LS")
		{
			CapData Data;
			user->Extend("CAP_REGHOLD");
			Data.type = "LS";
			Data.user = user;
			Data.creator = this->Creator;
			Data.parameter.clear();

			Event event((char*) &Data, (Module*)this->Creator, "cap_ls");
			event.Send(this->ServerInstance);
		}
		return CMD_FAILURE;
	}
};

class ModuleCAP : public Module
{
	CommandCAP* newcommand;
 public:
	ModuleCAP(InspIRCd* Me)
		: Module(Me)
	{
		// Create a new command
		newcommand = new CommandCAP(ServerInstance, this);
		ServerInstance->AddCommand(newcommand);

		Implementation eventlist[] = { I_OnCheckReady, I_OnCleanup, I_OnUserDisconnect, I_OnRequest };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}

	virtual bool OnCheckReady(User* user)
	{
		/* Users in CAP state get held until CAP END */
		if (user->GetExt("CAP_REGHOLD"))
			return true;

		return false;
	}

	virtual ~ModuleCAP()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleCAP)

