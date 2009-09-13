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

/* $ModDesc: Provides a SAKICK command */

/** Handle /SAKICK
 */
class CommandSakick : public Command
{
 public:
	CommandSakick(Module* Creator) : Command(Creator,"SAKICK", 2, 3)
	{
		flags_needed = 'o'; Penalty = 0; syntax = "<channel> <nick> [reason]";
		TRANSLATE4(TR_TEXT, TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[1]);
		Channel* channel = ServerInstance->FindChan(parameters[0]);
		const char* reason = "";
		const char* servername = NULL;

		if (dest && channel)
		{
			if (parameters.size() > 2)
			{
				reason = parameters[2].c_str();
			}
			else
			{
				reason = dest->nick.c_str();
			}

			if (ServerInstance->ULine(dest->server))
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Cannot use an SA command on a u-lined client", user->nick.c_str());
				return CMD_FAILURE;
			}

			/* For local clients, directly kick them. For remote clients,
			 * just return CMD_SUCCESS knowing the protocol module will route the SAKICK to the user's
			 * local server and that will kick them instead.
			 */
			if (IS_LOCAL(dest))
			{
				if (!channel->ServerKickUser(dest, reason, servername))
					delete channel;

				Channel *n = ServerInstance->FindChan(parameters[1]);
				if (n && n->HasUser(dest))
				{
					/* Sort-of-bug: If the command was issued remotely, this message won't be sent */
					user->WriteServ("NOTICE %s :*** Unable to kick %s from %s", user->nick.c_str(), dest->nick.c_str(), parameters[0].c_str());
					return CMD_FAILURE;
				}
			}

			if (IS_LOCAL(user))
			{
				/* Locally issued command; send the snomasks */
				ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick) + " SAKICKed " + dest->nick + " on " + parameters[0]);
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
		User* dest = ServerInstance->FindNick(parameters[1]);
		if (dest)
			return ROUTE_OPT_UCAST(dest->server);
		return ROUTE_LOCALONLY;
	}
};

class ModuleSakick : public Module
{
	CommandSakick cmd;
 public:
	ModuleSakick(InspIRCd* Me)
		: Module(Me), cmd(this)
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleSakick()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_OPTCOMMON|VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleSakick)

