/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
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

/// $ModAuthor: Attila Molnar
/// $ModAuthorMail: attilamolnar@hush.com
/// $ModDesc: Allows /STATS queries by name
/// $ModDepends: core 3

#include "inspircd.h"

class ModuleNamedStats : public Module
{
	typedef insp::flat_map<std::string, char, irc::insensitive_swo> NamedStatsMap;
	NamedStatsMap statsmap;

	void LoadDefaults()
	{
		statsmap["kline"] = 'k';
		statsmap["gline"] = 'g';
		statsmap["eline"] = 'e';
		statsmap["zline"] = 'Z';
		statsmap["qline"] = 'q';
		statsmap["filter"] = 's';
		statsmap["cban"] = 'C';
		statsmap["linkblock"] = 'c';
		statsmap["dnsbl"] = 'd';
		statsmap["conn"] = 'l';
		statsmap["cmd"] = 'm';
		statsmap["operacc"] = 'o';
		statsmap["opertype"] = 'O';
		statsmap["port"] = 'p';
		statsmap["uptime"] = 'u';
		statsmap["debug"] = 'z';
		statsmap["connectblock"] = 'I';
		statsmap["connectclass"] = 'i';
		statsmap["client"] = 'L';
		statsmap["oper"] = 'P';
		statsmap["socket"] = 'T';
		statsmap["uline"] = 'U';
		statsmap["connectclass"] = 'Y';
		statsmap["shun"] = 'H';
		statsmap["rline"] = 'R';
		statsmap["geoip"] = 'G';
		statsmap["svshold"] = 'S';
		statsmap["socketengine"] = 'E';
	}

 public:
	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) CXX11_OVERRIDE
	{
		if (!validated || command != "STATS" || parameters[0].length() == 1)
			return MOD_RES_PASSTHRU;

		NamedStatsMap::const_iterator it = statsmap.find(parameters[0]);
		if (it != statsmap.end())
			parameters[0] = it->second;

		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		statsmap.clear();
		if (ServerInstance->Config->ConfValue("namedstats")->getBool("enabledefaults", true))
			LoadDefaults();

		ConfigTagList tags = ServerInstance->Config->ConfTags("statsname");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string name = tag->getString("name");
			std::string ch = tag->getString("char");

			if ((!name.empty()) && (ch.length() == 1))
			{
				if (!statsmap.insert(std::make_pair(name, ch[0])).second)
					ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Name already exists in <statsname> entry at " + tag->getTagLocation());
			}
			else
			{
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Invalid name or char in <statsname> entry at " + tag->getTagLocation());
			}
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows /STATS queries by name");
	}
};

MODULE_INIT(ModuleNamedStats)
