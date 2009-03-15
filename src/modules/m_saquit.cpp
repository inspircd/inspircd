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

#include "inspircd.h"

/* $ModDesc: Provides support for an SAQUIT command, exits user with a reason */

/** Handle /SAQUIT
 */
class CommandSaquit : public Command
{
 public:
	CommandSaquit (InspIRCd* Instance) : Command(Instance, "SAQUIT", "o", 2, 2, false, 0)
	{
		this->source = "m_saquit.so";
		syntax = "<nick> <reason>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
		{
			if (ServerInstance->ULine(dest->server))
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Cannot use an SA command on a u-lined client",user->nick.c_str());
				return CMD_FAILURE;
			}

			ServerInstance->SNO->WriteToSnoMask('A', std::string(user->nick)+" used SAQUIT to make "+std::string(dest->nick)+" quit with a reason of "+parameters[1]);

			// Pass the command on, so the client's server can quit it properly.
			if (!IS_LOCAL(dest))
				return CMD_SUCCESS;

			ServerInstance->Users->QuitUser(dest, parameters[1]);
			return CMD_LOCALONLY;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick.c_str(), parameters[0].c_str());
		}

		return CMD_FAILURE;
	}
};

class ModuleSaquit : public Module
{
	CommandSaquit*	mycommand;
 public:
	ModuleSaquit(InspIRCd* Me)
		: Module(Me)
	{

		mycommand = new CommandSaquit(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~ModuleSaquit()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleSaquit)
