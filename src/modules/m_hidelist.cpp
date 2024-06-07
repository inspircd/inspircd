/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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
#include "numerichelper.h"

class ListWatcher final
	: public ModeWatcher
{
	// Minimum rank required to view the list
	const ModeHandler::Rank minrank;

public:
	ListWatcher(Module* mod, const std::string& modename, ModeHandler::Rank rank)
		: ModeWatcher(mod, modename, MODETYPE_CHANNEL)
		, minrank(rank)
	{
	}

	bool BeforeMode(User* user, User* destuser, Channel* chan, Modes::Change& change) override
	{
		// Only handle listmode list requests
		if (!change.param.empty())
			return true;

		// If the user requesting the list is a member of the channel see if they have the
		// rank required to view the list
		Membership* memb = chan->GetUser(user);
		if ((memb) && (memb->GetRank() >= minrank))
			return true;

		if (user->HasPrivPermission("channels/auspex"))
			return true;

		user->WriteNumeric(Numerics::ChannelPrivilegesNeeded(chan, minrank, fmt::format("view the channel {} list", GetModeName())));
		return false;
	}
};

class ModuleHideList final
	: public Module
{
	std::vector<ListWatcher*> watchers;

public:
	void ReadConfig(ConfigStatus& status) override
	{
		std::vector<std::pair<std::string, unsigned int>> newconfigs;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("hidelist"))
		{
			std::string modename = tag->getString("mode");
			if (modename.empty())
				throw ModuleException(this, "Empty <hidelist:mode> at " + tag->source.str());
			// If rank is set to 0 everyone inside the channel can view the list,
			// but non-members may not
			ModeHandler::Rank rank = tag->getNum<ModeHandler::Rank>("rank", HALFOP_VALUE);
			newconfigs.emplace_back(modename, rank);
		}

		stdalgo::delete_all(watchers);
		watchers.clear();

		for (const auto& [mode, rank] : newconfigs)
			watchers.push_back(new ListWatcher(this, mode, rank));
	}

	ModuleHideList()
		: Module(VF_VENDOR, "Allows list mode lists to be hidden from users without a prefix mode ranked equal to or higher than a defined level.")
	{
	}

	~ModuleHideList() override
	{
		stdalgo::delete_all(watchers);
	}
};

MODULE_INIT(ModuleHideList)
