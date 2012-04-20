/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2005 Craig Edwards <craigedwards@brainbox.cc>
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

		/* Do local sanity checks and bails */
		if (IS_LOCAL(user))
		{
			if (target && ServerInstance->ULine(target->server))
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Cannot use an SA command on a u-lined client",user->nick.c_str());
				return CMD_FAILURE;
			}

			if (!target)
			{
				user->WriteServ("NOTICE %s :*** No such nickname: '%s'", user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}

			if (!ServerInstance->IsNick(parameters[1].c_str(), ServerInstance->Config->Limits.NickMax))
			{
				user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick.c_str(), parameters[1].c_str());
				return CMD_FAILURE;
			}
		}

		/* Have we hit target's server yet? */
		if (target && IS_LOCAL(target))
		{
			std::string oldnick = user->nick;
			std::string newnick = target->nick;
			if (target->ForceNickChange(parameters[1].c_str()))
			{
				ServerInstance->SNO->WriteToSnoMask('a', oldnick+" used SANICK to change "+newnick+" to "+parameters[1]);
				ServerInstance->PI->SendSNONotice("A", oldnick+" used SANICK to change "+newnick+" to "+parameters[1]);
			}
			else
			{
				ServerInstance->SNO->WriteToSnoMask('a', oldnick+" failed SANICK (from "+newnick+" to "+parameters[1]+")");
				ServerInstance->PI->SendSNONotice("A", oldnick+" failed SANICK (from "+newnick+" to "+parameters[1]+")");
			}
			/* Yes, hit target and we have sent our NICK out, we can now bail */
			return CMD_LOCALONLY;
		}

		/* No, route it on */
		return CMD_SUCCESS;
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
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleSanick)
