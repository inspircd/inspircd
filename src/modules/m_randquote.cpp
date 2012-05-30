/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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


#include "inspircd.h"

static FileReader *quotes = NULL;

std::string q_file;
std::string prefix;
std::string suffix;

/* $ModDesc: Provides random quotes on connect. */

/** Handle /RANDQUOTE
 */
class CommandRandquote : public Command
{
 public:
	CommandRandquote (InspIRCd* Instance) : Command(Instance,"RANDQUOTE", 0, 0)
	{
		this->source = "m_randquote.so";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		std::string str;
		int fsize;

		if (q_file.empty() || quotes->Exists())
		{
			fsize = quotes->FileSize();
			str = quotes->GetLine(rand() % fsize);
			user->WriteServ("NOTICE %s :%s%s%s",user->nick.c_str(),prefix.c_str(),str.c_str(),suffix.c_str());
		}
		else
		{
			user->WriteServ("NOTICE %s :Your administrator specified an invalid quotes file, please bug them about this.", user->nick.c_str());
			return CMD_FAILURE;
		}

		return CMD_LOCALONLY;
	}
};

class ModuleRandQuote : public Module
{
 private:
	CommandRandquote* mycommand;
	ConfigReader *conf;
 public:
	ModuleRandQuote(InspIRCd* Me)
		: Module(Me)
	{

		conf = new ConfigReader(ServerInstance);
		// Sort the Randomizer thingie..
		srand(ServerInstance->Time());

		q_file = conf->ReadValue("randquote","file",0);
		prefix = conf->ReadValue("randquote","prefix",0);
		suffix = conf->ReadValue("randquote","suffix",0);

		mycommand = NULL;

		if (q_file.empty())
		{
			throw ModuleException("m_randquote: Quotefile not specified - Please check your config.");
		}

		quotes = new FileReader(ServerInstance, q_file);
		if(!quotes->Exists())
		{
			throw ModuleException("m_randquote: QuoteFile not Found!! Please check your config - module will not function.");
		}
		else
		{
			/* Hidden Command -- Mode clients assume /quote sends raw data to an IRCd >:D */
			mycommand = new CommandRandquote(ServerInstance);
			ServerInstance->AddCommand(mycommand);
		}
		Implementation eventlist[] = { I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	virtual ~ModuleRandQuote()
	{
		delete conf;
		delete quotes;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}

	virtual void OnUserConnect(User* user)
	{
		if (mycommand)
			mycommand->Handle(std::vector<std::string>(), user);
	}
};

MODULE_INIT(ModuleRandQuote)
