/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2003, 2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2005 Craig McLure <craig@chatspike.net>
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


/* $ModDesc: Provides random quotes on connect. */

#include "inspircd.h"

static FileReader *quotes = NULL;

std::string prefix;
std::string suffix;

/** Handle /RANDQUOTE
 */
class CommandRandquote : public Command
{
 public:
	CommandRandquote(Module* Creator) : Command(Creator,"RANDQUOTE", 0)
	{
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		int fsize = quotes->FileSize();
		if (fsize)
		{
			std::string str = quotes->GetLine(ServerInstance->GenRandomInt(fsize));
			if (!str.empty())
				user->WriteServ("NOTICE %s :%s%s%s",user->nick.c_str(),prefix.c_str(),str.c_str(),suffix.c_str());
		}

		return CMD_SUCCESS;
	}
};

class ModuleRandQuote : public Module
{
 private:
	CommandRandquote cmd;
 public:
	ModuleRandQuote()
		: cmd(this)
	{
	}

	void init()
	{
		ConfigTag* conf = ServerInstance->Config->ConfValue("randquote");

		std::string q_file = conf->getString("file","quotes");
		prefix = conf->getString("prefix");
		suffix = conf->getString("suffix");

		quotes = new FileReader(q_file);
		if (!quotes->Exists())
		{
			throw ModuleException("m_randquote: QuoteFile not Found!! Please check your config - module will not function.");
		}
		ServerInstance->Modules->AddService(cmd);
		Implementation eventlist[] = { I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}


	virtual ~ModuleRandQuote()
	{
		delete quotes;
	}

	virtual Version GetVersion()
	{
		return Version("Provides random quotes on connect.",VF_VENDOR);
	}

	virtual void OnUserConnect(LocalUser* user)
	{
		cmd.Handle(std::vector<std::string>(), user);
	}
};

MODULE_INIT(ModuleRandQuote)
