/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Robin Burchell <robin+git@viroteck.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
		User* u;
		if (IS_LOCAL(user))
			u = ServerInstance->FindNickOnly(parameters[0]);
		else
			u = ServerInstance->FindNick(parameters[0]);

		Channel* c = ServerInstance->FindChan(parameters[1]);

		if ((!c) || (!u) || (u->registered != REG_ALL))
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

		/* Servers remember invites only for their local users, so act
		 * only if the target is local. Otherwise the command will be
		 * passed to the target users server.
		 */
		LocalUser* lu = IS_LOCAL(u);
		if (lu)
		{
			irc::string xname(c->name.c_str());
			if (!lu->IsInvited(xname))
			{
				user->SendText(":%s 505 %s %s %s :Is not invited to channel %s", user->server.c_str(), user->nick.c_str(), u->nick.c_str(), c->name.c_str(), c->name.c_str());
				return CMD_FAILURE;
			}

			user->SendText(":%s 494 %s %s %s :Uninvited", user->server.c_str(), user->nick.c_str(), c->name.c_str(), u->nick.c_str());
			lu->RemoveInvite(xname);
			lu->WriteNumeric(493, "%s :You were uninvited from %s by %s", u->nick.c_str(), c->name.c_str(), user->nick.c_str());

			std::string msg = "*** " + user->nick + " uninvited " + u->nick + ".";
			c->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE " + c->name + " :" + msg);
			ServerInstance->PI->SendChannelNotice(c, 0, msg);
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
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
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

