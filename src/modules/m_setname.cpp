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

/* $ModDesc: Provides support for the SETNAME command */



class CommandSetname : public Command
{
 public:
	CommandSetname (InspIRCd* Instance) : Command(Instance,"SETNAME", 0, 1, 1)
	{
		this->source = "m_setname.so";
		syntax = "<new-gecos>";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		if (parameters.size() == 0)
		{
			user->WriteServ("NOTICE %s :*** SETNAME: GECOS must be specified", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (parameters[0].size() > ServerInstance->Config->Limits.MaxGecos)
		{
			user->WriteServ("NOTICE %s :*** SETNAME: GECOS too long", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (user->ChangeName(parameters[0].c_str()))
		{
			ServerInstance->SNO->WriteToSnoMask('a', "%s used SETNAME to change their GECOS to %s", user->nick.c_str(), parameters[0].c_str());
			return CMD_SUCCESS;
		}

		return CMD_SUCCESS;
	}
};


class ModuleSetName : public Module
{
	CommandSetname*	mycommand;
 public:
	ModuleSetName(InspIRCd* Me)
		: Module(Me)
	{

		mycommand = new CommandSetname(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~ModuleSetName()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleSetName)
