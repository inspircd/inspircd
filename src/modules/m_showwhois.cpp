/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2005, 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2004 Christopher Hall <typobox43@gmail.com>
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

/* $ModDesc: Allows opers to set +W to see when a user uses WHOIS on them */

/** Handle user mode +W
 */
class SeeWhois : public SimpleUserModeHandler
{
 public:
	SeeWhois(Module* Creator, bool IsOpersOnly) : SimpleUserModeHandler(Creator, "showwhois", 'W')
	{
		oper = IsOpersOnly;
	}
};

class WhoisNoticeCmd : public Command
{
 public:
	WhoisNoticeCmd(Module* Creator) : Command(Creator,"WHOISNOTICE", 2)
	{
		flags_needed = FLAG_SERVERONLY;
	}

	void HandleFast(User* dest, User* src)
	{
		dest->WriteServ("NOTICE %s :*** %s (%s@%s) did a /whois on you",
			dest->nick.c_str(), src->nick.c_str(), src->ident.c_str(),
			dest->HasPrivPermission("users/auspex") ? src->host.c_str() : src->dhost.c_str());
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (!dest)
			return CMD_FAILURE;

		User* source = ServerInstance->FindNick(parameters[1]);

		if (IS_LOCAL(dest) && source)
			HandleFast(dest, source);

		return CMD_SUCCESS;
	}
};

class ModuleShowwhois : public Module
{
	bool ShowWhoisFromOpers;
	SeeWhois* sw;
	WhoisNoticeCmd cmd;

 public:

	ModuleShowwhois()
		: sw(NULL), cmd(this)
	{
	}

	void init()
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("showwhois");

		bool OpersOnly = tag->getBool("opersonly", true);
		ShowWhoisFromOpers = tag->getBool("showfromopers", true);

		sw = new SeeWhois(this, OpersOnly);
		ServerInstance->Modules->AddService(*sw);
		ServerInstance->Modules->AddService(cmd);
		Implementation eventlist[] = { I_OnWhois };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	~ModuleShowwhois()
	{
		delete sw;
	}

	Version GetVersion()
	{
		return Version("Allows opers to set +W to see when a user uses WHOIS on them",VF_OPTCOMMON|VF_VENDOR);
	}

	void OnWhois(User* source, User* dest)
	{
		if (!dest->IsModeSet('W') || source == dest)
			return;

		if (!ShowWhoisFromOpers && IS_OPER(source))
			return;

		if (IS_LOCAL(dest))
		{
			cmd.HandleFast(dest, source);
		}
		else
		{
			std::vector<std::string> params;
			params.push_back(dest->server);
			params.push_back("WHOISNOTICE");
			params.push_back(dest->uuid);
			params.push_back(source->uuid);
			ServerInstance->PI->SendEncapsulatedData(params);
		}
	}

};

MODULE_INIT(ModuleShowwhois)
