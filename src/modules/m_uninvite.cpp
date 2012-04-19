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

/* $ModDesc: Provides the UNINVITE command which lets users un-invite other users from channels (!) */

#include "inspircd.h"

/** Handle /UNINVITE
 */
class CommandUninvite : public Command
{
 public:
	CommandUninvite(Module* Creator) : Command(Creator,"UNINVITE", 2)
	{
		syntax = "<nick> <channel>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		User* u = ServerInstance->FindNick(parameters[0]);
		Channel* c = ServerInstance->FindChan(parameters[1]);

		if ((!c) || (!u))
		{
			if (!c)
			{
				user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[1].c_str());
			}
			else
			{
				user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[0].c_str());
			}

			return CMD_FAILURE;
		}

		if (IS_LOCAL(user))
		{
			if (c->GetPrefixValue(user) < HALFOP_VALUE)
			{
				user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must be a channel %soperator", user->nick.c_str(), c->name.c_str(), c->GetPrefixValue(u) == HALFOP_VALUE ? "" : "half-");
				return CMD_FAILURE;
			}
		}

		irc::string xname(c->name.c_str());

		if (IS_LOCAL(u))
		{
			// TODO send messages & such out to remote servers
			LocalUser* lu = IS_LOCAL(u);
			if (!lu->IsInvited(xname))
			{
				user->WriteNumeric(505, "%s %s %s :Is not invited to channel %s", user->nick.c_str(), u->nick.c_str(), c->name.c_str(), c->name.c_str());
				return CMD_FAILURE;
			}
			user->WriteNumeric(494, "%s %s %s :Uninvited", user->nick.c_str(), c->name.c_str(), u->nick.c_str());
			lu->RemoveInvite(xname);
			lu->WriteNumeric(493, "%s :You were uninvited from %s by %s", u->nick.c_str(), c->name.c_str(), user->nick.c_str());
			c->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :*** %s uninvited %s.",
				c->name.c_str(), user->nick.c_str(), u->nick.c_str());
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* u = ServerInstance->FindNick(parameters[0]);
		return u ? ROUTE_OPT_UCAST(u->server) : ROUTE_LOCALONLY;
	}
};

class ModuleUninvite : public Module
{
	CommandUninvite cmd;

 public:

	ModuleUninvite() : cmd(this)
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleUninvite()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides the UNINVITE command which lets users un-invite other users from channels", VF_VENDOR | VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleUninvite)

