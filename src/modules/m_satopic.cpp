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

/* $ModDesc: Provides a SATOPIC command */

#include "inspircd.h"

/** Handle /SATOPIC
 */
class CommandSATopic : public Command
{
 public:
	CommandSATopic(Module* Creator) : Command(Creator,"SATOPIC", 2, 2)
	{
		flags_needed = 'o'; Penalty = 0; syntax = "<target> <topic>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		/*
		 * Handles a SATOPIC request. Notifies all +s users.
	 	 */
		Channel* target = ServerInstance->FindChan(parameters[0]);

		if(target)
		{
			std::string newTopic = parameters[1];

			// 3rd parameter overrides access checks
			target->SetTopic(user, newTopic, true);
			ServerInstance->SNO->WriteToSnoMask('a', user->nick + " used SATOPIC on " + target->name + ", new topic: " + newTopic);
			ServerInstance->PI->SendSNONotice("A", user->nick + " used SATOPIC on " + target->name + ", new topic: " + newTopic);

			return CMD_SUCCESS;
		}
		else
		{
			user->WriteNumeric(401, "%s %s :No such nick/channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
	}
};

class ModuleSATopic : public Module
{
	CommandSATopic cmd;
 public:
	ModuleSATopic()
	: cmd(this)
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleSATopic()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides a SATOPIC command", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSATopic)
