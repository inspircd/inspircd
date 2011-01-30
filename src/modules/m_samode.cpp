/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Provides more advanced UnrealIRCd SAMODE command */

#include "inspircd.h"
#include "command_parse.h"

/** Handle /SAMODE
 */
class CommandSamode : public Command
{
 public:
	bool active;
	CommandSamode(Module* Creator) : Command(Creator,"SAMODE", 2)
	{
		flags_needed = 'o'; Penalty = 0; syntax = "<target> <modes> {<mode-parameters>}";
		active = false;
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		this->active = true;
		ServerInstance->Parser->CallHandler("MODE", parameters, user);
		this->active = false;
		irc::stringjoiner list(" ", parameters, 0, parameters.size() - 1);
		ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick) + " used SAMODE: " + list.GetJoined());
		return CMD_SUCCESS;
	}
};

class ModuleSaMode : public Module
{
	CommandSamode cmd;
 public:
	ModuleSaMode() : cmd(this)
	{}

	void init()
	{
		ServerInstance->AddCommand(&cmd);
		ServerInstance->Modules->Attach(I_OnPreMode, this);
	}

	~ModuleSaMode()
	{
	}

	Version GetVersion()
	{
		return Version("Provides more advanced UnrealIRCd SAMODE command", VF_VENDOR);
	}

	ModResult OnPreMode(User*, Extensible*, irc::modestacker&)
	{
		if (cmd.active)
			return MOD_RES_ALLOW;
		return MOD_RES_PASSTHRU;
	}

	void Prioritize()
	{
		Module *override = ServerInstance->Modules->Find("m_override.so");
		ServerInstance->Modules->SetPriority(this, I_OnPreMode, PRIORITY_BEFORE, &override);
	}
};

MODULE_INIT(ModuleSaMode)
