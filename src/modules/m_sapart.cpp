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

/* $ModDesc: Provides support for unreal-style SAPART command */

/** Handle /SAPART
 */
class CommandSapart : public Command
{
 public:
	CommandSapart (InspIRCd* Instance, Module* Creator) : Command(Instance, Creator,"SAPART", "o", 2, 3, false, 0)
	{
		syntax = "<nick> <channel> [reason]";
		TRANSLATE4(TR_NICK, TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		Channel* channel = ServerInstance->FindChan(parameters[1]);
		std::string reason = "";

		if (dest && channel)
		{
			if (parameters.size() > 2)
				reason = parameters[2];

			if (ServerInstance->ULine(dest->server))
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Cannot use an SA command on a u-lined client",user->nick.c_str());
				return CMD_FAILURE;
			}

			/* For local clients, directly part them generating a PART message. For remote clients,
			 * just return CMD_SUCCESS knowing the protocol module will route the SAPART to the users
			 * local server and that will generate the PART instead
			 */
			if (IS_LOCAL(dest))
			{
				if (!channel->PartUser(dest, reason))
					delete channel;

				Channel* n = ServerInstance->FindChan(parameters[1]);
				if (!n)
				{
					ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick)+" used SAPART to make "+dest->nick+" part "+parameters[1]);
					return CMD_SUCCESS;
				}
				else
				{
					if (!n->HasUser(dest))
					{
						ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick)+" used SAPART to make "+dest->nick+" part "+parameters[1]);
						return CMD_SUCCESS;
					}
					else
					{
						user->WriteServ("NOTICE %s :*** Unable to make %s part %s",user->nick.c_str(), dest->nick.c_str(), parameters[1].c_str());
						return CMD_FAILURE;
					}
				}
			}

			return CMD_SUCCESS;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Invalid nickname or channel", user->nick.c_str());
		}

		return CMD_FAILURE;
	}
};


class ModuleSapart : public Module
{
	CommandSapart cmd;
 public:
	ModuleSapart(InspIRCd* Me)
		: Module(Me), cmd(Me, this)
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleSapart()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleSapart)

