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


#include "inspircd.h"

class ModuleRandQuote : public Module
{
 private:
	std::string prefix;
	std::string suffix;
	std::vector<std::string> quotes;

 public:
	void init() override
	{
		ConfigTag* conf = ServerInstance->Config->ConfValue("randquote");
		prefix = conf->getString("prefix");
		suffix = conf->getString("suffix");
		FileReader reader(conf->getString("file", "quotes"));
		quotes = reader.GetVector();
	}

	void OnUserConnect(LocalUser* user) override
	{
		if (!quotes.empty())
		{
			unsigned long random = ServerInstance->GenRandomInt(quotes.size());
			user->WriteNotice(prefix + quotes[random] + suffix);
		}
	}

	Version GetVersion() override
	{
		return Version("Provides random quotes on connect.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRandQuote)
