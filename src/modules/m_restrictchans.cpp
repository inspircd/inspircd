/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005-2006 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Only opers may create new channels if this module is loaded */

class ModuleRestrictChans : public Module
{
	std::set<irc::string> allowchans;

	void ReadConfig()
	{
		allowchans.clear();
		ConfigTagList tags = ServerInstance->Config->GetTags("allowchannel");
		for(ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string txt = tag->getString("name");
			allowchans.insert(txt.c_str());
		}
	}

 public:
	void init()
	{
		ReadConfig();
		Implementation eventlist[] = { I_OnCheckJoin };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ReadConfig();
	}


	void OnCheckJoin(ChannelPermissionData& join)
	{
		// channel does not yet exist (record is null, about to be created IF we were to allow it)
		if (!join.chan && join.result == MOD_RES_PASSTHRU && !IS_OPER(join.source))
		{
			// user is not an oper and its not in the allow list
			if (allowchans.find(join.channel) == allowchans.end())
			{
				join.ErrorNumeric(ERR_BANNEDFROMCHAN, "%s :Only IRC operators may create new channels",join.channel.c_str());
				join.result = MOD_RES_DENY;
			}
		}
	}

	virtual ~ModuleRestrictChans()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Only opers may create new channels if this module is loaded",VF_VENDOR);
	}
};

MODULE_INIT(ModuleRestrictChans)
