/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Implements config tags which allow blocking of joins to channels */

class ModuleDenyChannels : public Module
{
 public:
	ModuleDenyChannels()
	{}

	void init()
	{
		Implementation eventlist[] = { I_OnCheckJoin };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ConfigTag* FindBadChan(const std::string& cname)
	{
		ConfigTagList tags = ServerInstance->Config->GetTags("badchan");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			if (InspIRCd::Match(cname, i->second->getString("name")))
			{
				// found a bad channel. Maybe it has a goodchan pattern?
				tags = ServerInstance->Config->GetTags("goodchan");
				for (ConfigIter j = tags.first; j != tags.second; ++j)
				{
					if (InspIRCd::Match(cname, i->second->getString("name")))
						return NULL;
				}
				// nope
				return i->second;
			}
		}
		// no <badchan> found
		return NULL;
	}

	void ReadConfig(ConfigReadStatus& stat)
	{
		/* check for redirect validity and loops/chains */
		ConfigTagList tags = ServerInstance->Config->GetTags("badchan");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			std::string name = i->second->getString("name");
			std::string redirect = i->second->getString("redirect");

			if (!redirect.empty())
			{
				if (!ServerInstance->IsChannel(redirect.c_str(), ServerInstance->Config->Limits.ChanMax))
				{
					stat.ReportError(i->second, "Invalid badchan redirect, not a channel", true);
				}

				ConfigTag* tag = FindBadChan(redirect);

				if (tag)
				{
					/* <badchan:redirect> is a badchan */
					stat.ReportError(i->second, "Badchan redirect loop", true);
				}
			}
		}
	}

	virtual ~ModuleDenyChannels()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Implements config tags which allow blocking of joins to channels", VF_VENDOR);
	}


	void OnCheckJoin(ChannelPermissionData& join)
	{
		if (join.result != MOD_RES_PASSTHRU)
			return;
		ConfigTag* tag = FindBadChan(join.channel);
		if (!tag)
			return;
		if (IS_OPER(join.user) && tag->getBool("allowopers"))
			return;
		std::string reason = tag->getString("reason");
		std::string redirect = tag->getString("redirect");
		join.result = MOD_RES_DENY;

		if (!ServerInstance->RedirectJoin.get(join.user) && ServerInstance->IsChannel(redirect.c_str(), ServerInstance->Config->Limits.ChanMax))
		{
			join.user->WriteNumeric(926, "%s %s :Channel %s is forbidden, redirecting to %s: %s",
				join.user->nick.c_str(),join.channel.c_str(),join.channel.c_str(),redirect.c_str(), reason.c_str());
			ServerInstance->RedirectJoin.set(join.user, 1);
			Channel::JoinUser(join.user,redirect.c_str(),false,"",false,ServerInstance->Time());
			ServerInstance->RedirectJoin.set(join.user, 0);
		} else {
			join.user->WriteNumeric(926, "%s %s :Channel %s is forbidden: %s",
				join.user->nick.c_str(),join.channel.c_str(),join.channel.c_str(),reason.c_str());
		}
	}
};

MODULE_INIT(ModuleDenyChannels)
