/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014-2015 Attila Molnar <attilamolnar@hush.com>
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
#include "core_channel.h"
#include "invite.h"
#include "listmode.h"

class CoreModChannel : public Module
{
	Invite::APIImpl invapi;
	CommandInvite cmdinvite;
	CommandJoin cmdjoin;
	CommandKick cmdkick;
	CommandNames cmdnames;
	CommandTopic cmdtopic;

	ModResult IsInvited(User* user, Channel* chan)
	{
		LocalUser* localuser = IS_LOCAL(user);
		if ((localuser) && (invapi.IsInvited(localuser, chan)))
			return MOD_RES_ALLOW;
		return MOD_RES_PASSTHRU;
	}

 public:
	CoreModChannel()
		: invapi(this)
		, cmdinvite(this, invapi), cmdjoin(this), cmdkick(this), cmdnames(this), cmdtopic(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* optionstag = ServerInstance->Config->ConfValue("options");
		Implementation events[] = { I_OnCheckKey, I_OnCheckLimit, I_OnCheckChannelBan };
		if (optionstag->getBool("invitebypassmodes", true))
			ServerInstance->Modules.Attach(events, this, sizeof(events)/sizeof(Implementation));
		else
		{
			for (unsigned int i = 0; i < sizeof(events)/sizeof(Implementation); i++)
				ServerInstance->Modules.Detach(events[i], this);
		}
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		// Build a map of limits to their mode character.
		insp::flat_map<int, std::string> limits;
		const ModeParser::ListModeList& listmodes = ServerInstance->Modes->GetListModes();
		for (ModeParser::ListModeList::const_iterator iter = listmodes.begin(); iter != listmodes.end(); ++iter)
		{
			const unsigned int limit = (*iter)->GetLowerLimit();
			limits[limit].push_back((*iter)->GetModeChar());
		}

		// Generate the MAXLIST token from the limits map.
		std::string& buffer = tokens["MAXLIST"];
		for (insp::flat_map<int, std::string>::const_iterator iter = limits.begin(); iter != limits.end(); ++iter)
		{
			if (!buffer.empty())
				buffer.push_back(',');

			buffer.append(iter->second);
			buffer.push_back(':');
			buffer.append(ConvToStr(iter->first));
		}
	}

	void OnPostJoin(Membership* memb) CXX11_OVERRIDE
	{
		Channel* const chan = memb->chan;
		LocalUser* const localuser = IS_LOCAL(memb->user);
		if (localuser)
		{
			// Remove existing invite, if any
			invapi.Remove(localuser, chan);

			if (chan->topicset)
				Topic::ShowTopic(localuser, chan);

			// Show all members of the channel, including invisible (+i) users
			cmdnames.SendNames(localuser, chan, true);
		}
	}

	ModResult OnCheckKey(User* user, Channel* chan, const std::string& keygiven) CXX11_OVERRIDE
	{
		// Hook only runs when being invited bypasses +bkl
		return IsInvited(user, chan);
	}

	ModResult OnCheckChannelBan(User* user, Channel* chan) CXX11_OVERRIDE
	{
		// Hook only runs when being invited bypasses +bkl
		return IsInvited(user, chan);
	}

	ModResult OnCheckLimit(User* user, Channel* chan) CXX11_OVERRIDE
	{
		// Hook only runs when being invited bypasses +bkl
		return IsInvited(user, chan);
	}

	ModResult OnCheckInvite(User* user, Channel* chan) CXX11_OVERRIDE
	{
		// Hook always runs
		return IsInvited(user, chan);
	}

	void OnUserDisconnect(LocalUser* user) CXX11_OVERRIDE
	{
		invapi.RemoveAll(user);
	}

	void OnChannelDelete(Channel* chan) CXX11_OVERRIDE
	{
		// Make sure the channel won't appear in invite lists from now on, don't wait for cull to unset the ext
		invapi.RemoveAll(chan);
	}

	void Prioritize() CXX11_OVERRIDE
	{
		ServerInstance->Modules.SetPriority(this, I_OnPostJoin, PRIORITY_FIRST);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the INVITE, JOIN, KICK, NAMES, and TOPIC commands", VF_VENDOR|VF_CORE);
	}
};

MODULE_INIT(CoreModChannel)
