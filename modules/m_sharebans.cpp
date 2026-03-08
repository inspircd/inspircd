/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 Sadie Powell <sadie@witchery.services>
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
#include "listmode.h"
#include "modules/extban.h"
#include "numerichelper.h"

enum
{
	// InspIRCd-specific.
	ERR_BADCHANNEL = 926
};

class ShareExtBan final
	: public ExtBan::MatchingBase
{
private:
	ChanModeReference banmode;
	ChanModeReference secretmode;
	std::vector<Channel*> seen;

public:
	size_t maxdepth;

	ShareExtBan(Module* mod)
		: ExtBan::MatchingBase(mod, "share", 'b', ExtBan::MATCH_REQUIRE_CHANNEL)
		, banmode(mod, "ban")
		, secretmode(mod, "secret")
	{
	}

	bool IsMatch(ListModeBase* lm, User* user, Channel* channel, const std::string& text, const ExtBan::MatchConfig& config) override
	{
		if (seen.size() >= maxdepth)
			return false;

		auto banned = false;
		auto* targetchan = ServerInstance->Channels.Find(text);
		if (targetchan && targetchan != channel)
		{
			seen.push_back(channel);
			banned = targetchan->CheckList(*banmode, user, config.match_real_mask);
			std::erase(seen, channel);
		}
		return banned;
	}

	bool Validate(ListModeBase* lm, LocalUser* user, Channel* channel, std::string& text) override
	{
		auto* targetchan = ServerInstance->Channels.Find(text);
		if (!targetchan || (targetchan->IsModeSet(secretmode) && (!targetchan->HasUser(user) && !user->HasPrivPermission("channels/auspex"))))
		{
			user->WriteNumeric(Numerics::NoSuchChannel(text));
			return false;
		}

		if (targetchan == channel)
		{
			user->WriteNumeric(ERR_BADCHANNEL, text, "You can not create a channel ban loop.");
			return false;
		}

		auto* memb = targetchan->GetUser(user);
		if (!memb)
		{
			user->WriteNumeric(ERR_NOTONCHANNEL, targetchan->name, "You're not on that channel!");
			return false;
		}

		const auto rank = lm->GetLevelRequired(true);
		if (memb->GetRank() < rank)
		{
			user->WriteNumeric(Numerics::ChannelPrivilegesNeeded(targetchan, rank, "share bans between channels"));
			return false;
		}

		return true;
	}
};

class ModuleShareBans final
	: public Module
{
private:
	ShareExtBan extban;

public:
	ModuleShareBans()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds extended ban b: (share) which allows sharing bans between channels.")
		, extban(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("sharebans");
		extban.maxdepth = tag->getNum<size_t>("maxdepth", 2, 1, 100);
	}
};

MODULE_INIT(ModuleShareBans)
