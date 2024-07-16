/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2019-2021, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
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

class ModuleRandQuote final
	: public Module
{
private:
	std::string prefix;
	std::string suffix;
	std::vector<std::string> quotes;

public:
	ModuleRandQuote()
		: Module(VF_VENDOR, "Allows random quotes to be sent to users when they connect to the server.")
	{
	}

	void init() override
	{
		const auto& conf = ServerInstance->Config->ConfValue("randquote");
		prefix = conf->getString("prefix");
		suffix = conf->getString("suffix");

		const std::string filestr = conf->getString("file", "quotes", 1);
		auto file = ServerInstance->Config->ReadFile(filestr);
		if (!file)
			throw ModuleException(this, "Unable to read quotes from " + filestr + ": " + file.error);

		std::vector<std::string> newquotes;
		irc::sepstream linestream(file.contents, '\n');
		for (std::string line; linestream.GetToken(line); )
			newquotes.push_back(line);
		std::swap(quotes, newquotes);
	}

	void OnUserConnect(LocalUser* user) override
	{
		if (!quotes.empty())
		{
			unsigned long random = ServerInstance->GenRandomInt(quotes.size());
			user->WriteNotice(prefix + quotes[random] + suffix);
		}
	}
};

MODULE_INIT(ModuleRandQuote)
