/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "clientprotocolevent.h"

namespace
{
class Settings final
{
	typedef insp::flat_map<std::string, ModeHandler::Rank> RanksToSeeMap;
	RanksToSeeMap rankstosee;

public:
	ModeHandler::Rank GetRequiredRank(const ModeHandler& mh) const
	{
		RanksToSeeMap::const_iterator it = rankstosee.find(mh.name);
		if (it != rankstosee.end())
			return it->second;
		return 0;
	}

	void Load(const Module* mod)
	{
		RanksToSeeMap newranks;

		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("hidemode"))
		{
			const std::string modename = tag->getString("mode");
			if (modename.empty())
				throw ModuleException(mod, "<hidemode:mode> is empty at " + tag->source.str());

			ModeHandler::Rank rank = tag->getNum<ModeHandler::Rank>("rank", 0);
			if (!rank)
				throw ModuleException(mod, "<hidemode:rank> must be greater than 0 at " + tag->source.str());

			ServerInstance->Logs.Debug(MODNAME, "Hiding the {} mode from users below rank {}", modename, rank);
			newranks.emplace(modename, rank);
		}
		rankstosee.swap(newranks);
	}
};

class ModeHook final
	: public ClientProtocol::EventHook
{
	typedef insp::flat_map<ModeHandler::Rank, const ClientProtocol::MessageList*> FilteredModeMap;

	std::vector<Modes::ChangeList> modechangelists;
	std::list<ClientProtocol::Messages::Mode> filteredmodelist;
	std::list<ClientProtocol::MessageList> filteredmsgplists;
	FilteredModeMap cache;

	static ModResult HandleResult(const ClientProtocol::MessageList* filteredmessagelist, ClientProtocol::MessageList& messagelist)
	{
		// Deny if member isn't allowed to see even a single mode change from this mode event
		if (!filteredmessagelist)
			return MOD_RES_DENY;

		// Member is allowed to see at least one mode change, replace list
		if (filteredmessagelist != &messagelist)
			messagelist = *filteredmessagelist;

		return MOD_RES_PASSTHRU;
	}

	Modes::ChangeList* FilterModeChangeList(const ClientProtocol::Events::Mode& mode, ModeHandler::Rank rank)
	{
		Modes::ChangeList* modechangelist = nullptr;
		for (Modes::ChangeList::List::const_iterator i = mode.GetChangeList().getlist().begin(); i != mode.GetChangeList().getlist().end(); ++i)
		{
			const Modes::Change& curr = *i;
			if (settings.GetRequiredRank(*curr.mh) <= rank)
			{
				// No restriction on who can see this mode or there is one but the member's rank is sufficient
				if (modechangelist)
					modechangelist->push(curr);

				continue;
			}

			// Member cannot see the current mode change

			if (!modechangelist)
			{
				// Create new mode change list or reuse the last one if it's empty
				if ((modechangelists.empty()) || (!modechangelists.back().empty()))
					modechangelists.emplace_back();

				// Add all modes to it which we've accepted so far
				modechangelists.back().push(mode.GetChangeList().getlist().begin(), i);
				modechangelist = &modechangelists.back();
			}
		}
		return modechangelist;
	}

	void OnEventInit(const ClientProtocol::Event& ev) override
	{
		cache.clear();
		filteredmsgplists.clear();
		filteredmodelist.clear();
		modechangelists.clear();

		// Ensure no reallocations will happen
		const size_t numprefixmodes = ServerInstance->Modes.GetPrefixModes().size();
		modechangelists.reserve(numprefixmodes);
	}

	ModResult OnPreEventSend(LocalUser* user, const ClientProtocol::Event& ev, ClientProtocol::MessageList& messagelist) override
	{
		const ClientProtocol::Events::Mode& mode = static_cast<const ClientProtocol::Events::Mode&>(ev);
		Channel* const chan = mode.GetMessages().front().GetChanTarget();
		if (!chan)
			return MOD_RES_PASSTHRU;

		if (user->HasPrivPermission("channels/auspex"))
			return MOD_RES_PASSTHRU;

		Membership* const memb = chan->GetUser(user);
		if (!memb)
			return MOD_RES_PASSTHRU;

		// Check cache first
		const FilteredModeMap::const_iterator it = cache.find(memb->GetRank());
		if (it != cache.end())
			return HandleResult(it->second, messagelist);

		// Message for this rank isn't cached, generate it now
		const Modes::ChangeList* const filteredchangelist = FilterModeChangeList(mode, memb->GetRank());

		// If no new change list was generated (above method returned NULL) it means the member and everyone else
		// with the same rank can see everything in the original change list.
		ClientProtocol::MessageList* finalmsgplist = &messagelist;
		if (filteredchangelist)
		{
			if (filteredchangelist->empty())
			{
				// This rank cannot see any mode changes in the original change list
				finalmsgplist = nullptr;
			}
			else
			{
				// This rank can see some of the mode changes in the filtered mode change list.
				// Create and store a new protocol message from it.
				filteredmsgplists.emplace_back();
				ClientProtocol::Events::Mode::BuildMessages(mode.GetMessages().front().GetSourceUser(), chan, nullptr, *filteredchangelist, filteredmodelist, filteredmsgplists.back());
				finalmsgplist = &filteredmsgplists.back();
			}
		}

		// Cache the result in all cases so it can be reused for further members with the same rank
		cache.emplace(memb->GetRank(), finalmsgplist);
		return HandleResult(finalmsgplist, messagelist);
	}

public:
	Settings settings;

	ModeHook(Module* mod)
		: ClientProtocol::EventHook(mod, "MODE", 10)
	{
	}
};
}

class ModuleHideMode final
	: public Module
{
private:
	ModeHook modehook;

public:
	ModuleHideMode()
		: Module(VF_VENDOR, "Allows mode changes to be hidden from users without a prefix mode ranked equal to or higher than a defined level.")
		, modehook(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		modehook.settings.Load(this);
	}
};

MODULE_INIT(ModuleHideMode)
