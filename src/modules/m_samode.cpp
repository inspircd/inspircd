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

/* $ModDesc: Provides more advanced UnrealIRCd SAMODE command */

#include "inspircd.h"

/** Handle /SAMODE
 */
class CommandSamode : public Command
{
 public:
	bool active;
	CommandSamode (InspIRCd* Instance, Module* Creator) : Command(Instance, Creator,"SAMODE", "o", 2, false, 0)
	{
		syntax = "<target> <modes> {<mode-parameters>}";
		active = false;
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		this->active = true;
		ServerInstance->Parser->CallHandler("MODE", parameters, user);
		if (ServerInstance->Modes->GetLastParse().length())
			ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick) + " used SAMODE: " +ServerInstance->Modes->GetLastParse());
		this->active = false;
		return CMD_LOCALONLY;
	}
};

class ModuleSaMode : public Module
{
	CommandSamode cmd;
 public:
	ModuleSaMode(InspIRCd* Me)
		: Module(Me), cmd(Me, this)
	{
		ServerInstance->AddCommand(&cmd);
		ServerInstance->Modules->Attach(I_OnAccessCheck, this);
	}

	virtual ~ModuleSaMode()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

	virtual ModResult OnAccessCheck(User* source,User* dest,Channel* channel,int access_type)
	{
		if (cmd.active)
			return MOD_RES_ALLOW;
		return MOD_RES_PASSTHRU;
	}

	void Prioritize()
	{
		Module *override = ServerInstance->Modules->Find("m_override.so");
		ServerInstance->Modules->SetPriority(this, I_OnAccessCheck, PRIORITY_BEFORE, &override);
	}
};

MODULE_INIT(ModuleSaMode)
