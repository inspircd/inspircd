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

#include "inspircd.h"

/* $ModDesc: Provides support for SANICK command */

/** Handle /SANICK
 */
class CommandSanick : public Command
{
 public:
	CommandSanick (InspIRCd* Instance) : Command(Instance,"SANICK", 'o', 2, false, 0)
	{
		this->source = "m_sanick.so";
		syntax = "<nick> <new-nick>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		User* source = ServerInstance->FindNick(parameters[0]);
		if (source)
		{
			if (ServerInstance->ULine(source->server))
			{
				user->WriteServ("990 %s :Cannot use an SA command on a u-lined client",user->nick);
				return CMD_FAILURE;
			}
			std::string oldnick = user->nick;
			if (ServerInstance->IsNick(parameters[1]))
			{
				if (source->ForceNickChange(parameters[1]))
				{
					ServerInstance->WriteOpers("*** " + oldnick+" used SANICK to change "+std::string(parameters[0])+" to "+parameters[1]);
					return CMD_SUCCESS;
				}
				else
				{
					/* We couldnt change the nick */
					ServerInstance->WriteOpers("*** " + oldnick+" failed SANICK (from "+std::string(parameters[0])+" to "+parameters[1]+")");
					return CMD_FAILURE;
				}
			}
			else
			{
				user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick, parameters[1]);
			}

			return CMD_FAILURE;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** No such nickname: '%s'", user->nick, parameters[0]);
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
		return Version(1, 1, 0, 1, VF_COMMON | VF_VENDOR, API_VERSION);
	}
	
};

MODULE_INIT(ModuleSanick)
