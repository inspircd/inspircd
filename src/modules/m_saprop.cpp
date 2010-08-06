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

/* $ModDesc: Provides the SAPROP command */

#include "inspircd.h"

/** Handle /SAPROP
 */
class CommandSaprop : public Command
{
 public:
	bool active;
	CommandSaprop(Module* Creator) : Command(Creator,"SAPROP", 2)
	{
		flags_needed = 'o'; Penalty = 0; syntax = "<user|channel> {[+-]<mode> [<value>]}*";
		active = false;
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		this->active = true;
		ServerInstance->Parser->CallHandler("PROP", parameters, user);
		this->active = false;
		irc::stringjoiner list(" ", parameters, 0, parameters.size() - 1);
		ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick) + " used SAPROP: " + list.GetJoined());
		return CMD_SUCCESS;
	}
};

class ModuleSaProp : public Module
{
	CommandSaprop cmd;
 public:
	ModuleSaProp() : cmd(this) {}

	void init()
	{
		ServerInstance->AddCommand(&cmd);
		ServerInstance->Modules->Attach(I_OnPreMode, this);
	}

	Version GetVersion()
	{
		return Version("Provides the SAPROP command", VF_VENDOR);
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

MODULE_INIT(ModuleSaProp)
