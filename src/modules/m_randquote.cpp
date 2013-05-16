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

class ModuleRandQuote : public Module
{
private:
	std::string prefix;
	std::string suffix;
	std::vector<std::string> quotes;

 public:
	void init() CXX11_OVERRIDE
	{
		ConfigTag* conf = ServerInstance->Config->ConfValue("randquote");
		prefix = conf->getString("prefix");
		suffix = conf->getString("suffix");
		std::string quoteFile = conf->getString("file", "conf/quotes.txt");
		if (!ServerConfig::FileExists(quoteFile.c_str()))
		{
			throw ModuleException("m_randquote: quote file '" + quoteFile + "' does not exist!");
		}
		std::ifstream stream(quoteFile.c_str());
		if (!stream.is_open())
		{
			throw ModuleException("m_randquote: quote file '" + quoteFile + "' is not readable!");
		}
		std::string line;
		while (std::getline(stream, line))
		{
			quotes.push_back(line);
		}
		stream.close();
		Implementation eventlist[] = { I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnUserConnect(LocalUser* user) CXX11_OVERRIDE
	{
		if (!quotes.empty())
		{
			unsigned long random = ServerInstance->GenRandomInt(quotes.size());
			user->WriteNotice(prefix + quotes[random] + suffix);
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides random quotes on connect.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRandQuote)
