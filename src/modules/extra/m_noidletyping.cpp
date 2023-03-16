/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDepends: core 3
/// $ModDesc: Prevents typing notifications from being sent to idle users.


#include "inspircd.h"
#include "modules/ctctags.h"

class ModuleNoIdleTyping
	: public Module
	, public CTCTags::EventListener
{
 private:
	unsigned long duration;

	bool IsIdle(User* source)
	{
		LocalUser* lsource = IS_LOCAL(source);
		if (!lsource)
		{
			// Servers handle their own users.
			return false;
		}

		unsigned long diff = ServerInstance->Time() - lsource->idle_lastmsg;
		return diff > duration;
	}

	ModResult BuildChannelExempts(User* source, Channel* channel, CTCTags::TagMessageDetails& details)
	{
		const Channel::MemberMap& members = channel->GetUsers();
		for (Channel::MemberMap::const_iterator member = members.begin(); member != members.end(); ++member)
		{
			if (IsIdle(source))
				details.exemptions.insert(member->first);
		}
		return MOD_RES_PASSTHRU;
	}

 public:
	ModuleNoIdleTyping()
		: CTCTags::EventListener(this, 200)
	{
	}

	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("noidletyping");
		duration = tag->getDuration("duration", 60*10, 60);
	}

	ModResult OnUserPreTagMessage(User* user, const MessageTarget& target, CTCTags::TagMessageDetails& details) CXX11_OVERRIDE
	{
		ClientProtocol::TagMap::const_iterator iter = details.tags_out.find("+typing");
		ClientProtocol::TagMap::const_iterator draftiter = details.tags_out.find("+draft/typing");
		if (iter == details.tags_out.end() && draftiter == details.tags_out.end())
			return MOD_RES_PASSTHRU;

		switch (target.type)
		{
			case MessageTarget::TYPE_CHANNEL:
				return BuildChannelExempts(user, target.Get<Channel>(), details);

			case MessageTarget::TYPE_USER:
				return IsIdle(target.Get<User>()) ? MOD_RES_DENY : MOD_RES_PASSTHRU;

			default:
				return MOD_RES_PASSTHRU;
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Prevents typing notifications from being sent to idle users.");
	}
};

MODULE_INIT(ModuleNoIdleTyping)

