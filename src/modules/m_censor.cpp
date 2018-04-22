/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2004, 2008-2009 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007 Robin Burchell <robin+git@viroteck.net>
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
#include "modules/exemption.h"

typedef insp::flat_map<std::string, std::string, irc::insensitive_swo> censor_t;

class ModuleCensor : public Module
{
	CheckExemption::EventProvider exemptionprov;
	censor_t censors;
	SimpleUserModeHandler cu;
	SimpleChannelModeHandler cc;

 public:
	ModuleCensor()
		: exemptionprov(this)
		, cu(this, "u_censor", 'G')
		, cc(this, "censor", 'G')
	{
	}

	// format of a config entry is <badword text="shit" replace="poo">
	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		bool active = false;

		if (target.type == MessageTarget::TYPE_USER)
			active = target.Get<User>()->IsModeSet(cu);
		else if (target.type == MessageTarget::TYPE_CHANNEL)
		{
			Channel* c = target.Get<Channel>();
			active = c->IsModeSet(cc);
			ModResult res = CheckExemption::Call(exemptionprov, user, c, "censor");

			if (res == MOD_RES_ALLOW)
				return MOD_RES_PASSTHRU;
		}

		if (!active)
			return MOD_RES_PASSTHRU;

		for (censor_t::iterator index = censors.begin(); index != censors.end(); index++)
		{
			size_t censorpos;
			while ((censorpos = irc::find(details.text, index->first)) != std::string::npos)
			{
				if (index->second.empty())
				{
					const std::string targname = target.type == MessageTarget::TYPE_CHANNEL ? target.Get<Channel>()->name : target.Get<User>()->nick;
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, targname, index->first.c_str(), "Your message contained a censored word, and was blocked");
					return MOD_RES_DENY;
				}

				details.text.replace(censorpos, index->first.size(), index->second);
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		/*
		 * reload our config file on rehash - we must destroy and re-allocate the classes
		 * to call the constructor again and re-read our data.
		 */
		censors.clear();

		ConfigTagList badwords = ServerInstance->Config->ConfTags("badword");
		for (ConfigIter i = badwords.first; i != badwords.second; ++i)
		{
			ConfigTag* tag = i->second;
			const std::string text = tag->getString("text");
			if (text.empty())
				continue;

			const std::string replace = tag->getString("replace");
			censors[text] = replace;
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides user and channel +G mode",VF_VENDOR);
	}

};

MODULE_INIT(ModuleCensor)
