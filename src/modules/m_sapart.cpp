/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
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
	CommandSapart(Module* Creator) : Command(Creator,"SAPART", 2, 3)
	{
		flags_needed = 'o'; Penalty = 0; syntax = "<nick> <channel> [reason]";
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
				channel->PartUser(dest, reason);

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

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
			return ROUTE_OPT_UCAST(dest->server);
		return ROUTE_LOCALONLY;
	}
};


class ModuleSapart : public Module
{
	CommandSapart cmd;
 public:
	ModuleSapart()
		: cmd(this)
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleSapart()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for unreal-style SAPART command", VF_OPTCOMMON | VF_VENDOR);
	}

};

MODULE_INIT(ModuleSapart)

