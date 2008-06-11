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

/* $ModDesc: Provides support for SANICK command */

/** Handle /SANICK
 */
class CommandSanick : public Command
{
 public:
	CommandSanick (InspIRCd* Instance) : Command(Instance,"SANICK", "o", 2, false, 0)
	{
		this->source = "m_sanick.so";
		syntax = "<nick> <new-nick>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* target = ServerInstance->FindNick(parameters[0]);
		if (target)
		{
			if (ServerInstance->ULine(target->server))
			{
				user->WriteNumeric(990, "%s :Cannot use an SA command on a u-lined client",user->nick.c_str());
				return CMD_FAILURE;
			}
			std::string oldnick = user->nick;
			if (IS_LOCAL(user) && !ServerInstance->IsNick(parameters[1].c_str(), ServerInstance->Config->Limits.NickMax))
			{
				user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick.c_str(), parameters[1].c_str());
			}
			else
			{
				if (target->ForceNickChange(parameters[1].c_str()))
				{
					ServerInstance->SNO->WriteToSnoMask('A', oldnick+" used SANICK to change "+parameters[0]+" to "+parameters[1]);
					return CMD_SUCCESS;
				}
				else
				{
					/* We couldnt change the nick */
					ServerInstance->SNO->WriteToSnoMask('A', oldnick+" failed SANICK (from "+parameters[0]+" to "+parameters[1]+")");
					return CMD_FAILURE;
				}
			}

			return CMD_FAILURE;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** No such nickname: '%s'", user->nick.c_str(), parameters[0].c_str());
		}

		return CMD_FAILURE;
	}
};


class ModuleSanick : public Module
{
	CommandSanick*	mycommand;
 public:
	ModuleSanick(InspIRCd* Me)
		: Module(Me)
	{

		mycommand = new CommandSanick(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~ModuleSanick()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1, 2, 0, 1, VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleSanick)
