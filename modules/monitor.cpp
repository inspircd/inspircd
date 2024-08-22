/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 delthas
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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
#include "modules/cap.h"
#include "modules/isupport.h"
#include "modules/monitor.h"
#include "numericbuilder.h"

namespace IRCv3::Monitor
{
	class ExtItem;
	struct Entry;
	class Manager;
	class ManagerInternal;

	typedef std::vector<Entry*> WatchedList;
	typedef std::vector<LocalUser*> WatcherList;
}

struct IRCv3::Monitor::Entry final
{
	WatcherList watchers;
	std::string nick;

	void SetNick(const std::string& Nick)
	{
		nick.clear();
		// We may show this string to other users so do not leak the casing
		std::transform(Nick.begin(), Nick.end(), std::back_inserter(nick), ::tolower);
	}

	const std::string& GetNick() const { return nick; }
};

class IRCv3::Monitor::Manager final
{
	struct ExtData final
	{
		WatchedList list;
	};

	class ExtItem final
		: public ExtensionItem
	{
		Manager& manager;

	public:
		ExtItem(Module* mod, const std::string& extname, Manager& managerref)
			: ExtensionItem(mod, extname, ExtensionType::USER)
			, manager(managerref)
		{
		}

		ExtData* Get(User* user, bool create = false)
		{
			ExtData* extdata = static_cast<ExtData*>(GetRaw(user));
			if ((!extdata) && (create))
			{
				extdata = new ExtData;
				SetRaw(user, extdata);
			}
			return extdata;
		}

		void Unset(User* user)
		{
			Delete(user, UnsetRaw(user));
		}

		std::string ToInternal(const Extensible* container, void* item) const noexcept override
		{
			std::string ret;
			const ExtData* extdata = static_cast<ExtData*>(item);
			for (const auto* entry : extdata->list)
				ret.append(entry->GetNick()).push_back(' ');
			if (!ret.empty())
				ret.pop_back();
			return ret;
		}

		void FromInternal(Extensible* container, const std::string& value) noexcept override;

		void Delete(Extensible* container, void* item) override
		{
			delete static_cast<ExtData*>(item);
		}
	};

public:
	Manager(Module* mod, const std::string& extname)
		: ext(mod, extname, *this)
	{
	}

	enum WatchResult
	{
		WR_OK,
		WR_TOOMANY,
		WR_ALREADYWATCHING,
		WR_INVALIDNICK
	};

	WatchResult Watch(LocalUser* user, const std::string& nick, unsigned long maxwatch)
	{
		if (!ServerInstance->IsNick(nick))
			return WR_INVALIDNICK;

		WatchedList* watched = GetWatchedPriv(user, true);
		if (watched->size() >= maxwatch)
			return WR_TOOMANY;

		Entry* entry = AddWatcher(nick, user);
		if (stdalgo::isin(*watched, entry))
			return WR_ALREADYWATCHING;

		entry->watchers.push_back(user);
		watched->push_back(entry);
		return WR_OK;
	}

	bool Unwatch(LocalUser* user, const std::string& nick)
	{
		WatchedList* list = GetWatchedPriv(user);
		if (!list)
			return false;

		bool ret = RemoveWatcher(nick, user, *list);
		// If no longer watching any nick unset ext
		if (list->empty())
			ext.Unset(user);
		return ret;
	}

	const WatchedList& GetWatched(LocalUser* user)
	{
		WatchedList* list = GetWatchedPriv(user);
		if (list)
			return *list;
		return emptywatchedlist;
	}

	void UnwatchAll(LocalUser* user)
	{
		WatchedList* list = GetWatchedPriv(user);
		if (!list)
			return;

		while (!list->empty())
		{
			Entry* entry = list->front();
			RemoveWatcher(entry->GetNick(), user, *list);
		}
		ext.Unset(user);
	}

	WatcherList* GetWatcherList(const std::string& nick)
	{
		Entry* entry = Find(nick);
		if (entry)
			return &entry->watchers;
		return nullptr;
	}

private:
	typedef std::unordered_map<std::string, Entry, irc::insensitive, irc::StrHashComp> NickHash;

	Entry* Find(const std::string& nick)
	{
		NickHash::iterator it = nicks.find(nick);
		if (it != nicks.end())
			return &it->second;
		return nullptr;
	}

	Entry* AddWatcher(const std::string& nick, LocalUser* user)
	{
		std::pair<NickHash::iterator, bool> ret = nicks.emplace(nick, Entry());
		Entry& entry = ret.first->second;
		if (ret.second)
			entry.SetNick(nick);
		return &entry;
	}

	bool RemoveWatcher(const std::string& nick, LocalUser* user, WatchedList& watchedlist)
	{
		NickHash::iterator it = nicks.find(nick);
		// If nobody is watching this nick the user trying to remove it isn't watching it for sure
		if (it == nicks.end())
			return false;

		Entry& entry = it->second;
		// Erase from the user's list of watched nicks
		if (!stdalgo::vector::swaperase(watchedlist, &entry))
			return false; // User is not watching this nick

		// Erase from the nick's list of watching users
		stdalgo::vector::swaperase(entry.watchers, user);

		// If nobody else is watching the nick remove map entry
		if (entry.watchers.empty())
			nicks.erase(it);

		return true;
	}

	WatchedList* GetWatchedPriv(LocalUser* user, bool create = false)
	{
		ExtData* extdata = ext.Get(user, create);
		if (!extdata)
			return nullptr;
		return &extdata->list;
	}

	NickHash nicks;
	ExtItem ext;
	WatchedList emptywatchedlist;
};

void IRCv3::Monitor::Manager::ExtItem::FromInternal(Extensible* container, const std::string& value) noexcept
{
	if (container->extype != this->extype)
		return;

	irc::spacesepstream ss(value);
	for (std::string nick; ss.GetToken(nick); )
		manager.Watch(static_cast<LocalUser*>(container), nick, ULONG_MAX);
}

#ifndef INSPIRCD_MONITOR_MANAGER_ONLY

enum
{
	RPL_MONONLINE = 730,
	RPL_MONOFFLINE = 731,
	RPL_MONLIST = 732,
	RPL_ENDOFMONLIST = 733,
	ERR_MONLISTFULL = 734
};

class CommandMonitor final
	: public SplitCommand
{
	typedef Numeric::Builder<> ReplyBuilder;
	// Additional penalty for the /MONITOR L and /MONITOR S commands that request a list from the server
	static constexpr unsigned int ListPenalty = 3000;

	IRCv3::Monitor::Manager& manager;

	void HandlePlus(LocalUser* user, const std::string& input)
	{
		ReplyBuilder online(user, RPL_MONONLINE);
		ReplyBuilder offline(user, RPL_MONOFFLINE);
		irc::commasepstream ss(input);
		for (std::string nick; ss.GetToken(nick); )
		{
			IRCv3::Monitor::Manager::WatchResult result = manager.Watch(user, nick, maxmonitor);
			if (result == IRCv3::Monitor::Manager::WR_TOOMANY)
			{
				// List is full, send error which includes the remaining nicks that were not processed
				user->WriteNumeric(ERR_MONLISTFULL, maxmonitor, FMT::format("{}{}{}", nick, (ss.StreamEnd() ? "" : ","), ss.GetRemaining()), "Monitor list is full");
				break;
			}
			else if (result != IRCv3::Monitor::Manager::WR_OK)
				continue; // Already added or invalid nick

			ReplyBuilder& out = (ServerInstance->Users.FindNick(nick, true) ? online : offline);
			out.Add(nick);
		}

		online.Flush();
		offline.Flush();
	}

	void HandleMinus(LocalUser* user, const std::string& input)
	{
		irc::commasepstream ss(input);
		for (std::string nick; ss.GetToken(nick); )
			manager.Unwatch(user, nick);
	}

public:
	unsigned long maxmonitor;

	CommandMonitor(Module* mod, IRCv3::Monitor::Manager& managerref)
		: SplitCommand(mod, "MONITOR", 1)
		, manager(managerref)
	{
		penalty = 2000;
		syntax = { "C", "L", "S", "(+|-) <nick>[,<nick>]+" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		char subcmd = toupper(parameters[0][0]);
		if (subcmd == '+')
		{
			if (parameters.size() > 1)
				HandlePlus(user, parameters[1]);
		}
		else if (subcmd == '-')
		{
			if (parameters.size() > 1)
				HandleMinus(user, parameters[1]);
		}
		else if (subcmd == 'C')
		{
			manager.UnwatchAll(user);
		}
		else if (subcmd == 'L')
		{
			user->CommandFloodPenalty += ListPenalty;
			const IRCv3::Monitor::WatchedList& list = manager.GetWatched(user);
			ReplyBuilder out(user, RPL_MONLIST);
			for (const auto* entry : list)
				out.Add(entry->GetNick());
			out.Flush();
			user->WriteNumeric(RPL_ENDOFMONLIST, "End of MONITOR list");
		}
		else if (subcmd == 'S')
		{
			user->CommandFloodPenalty += ListPenalty;

			ReplyBuilder online(user, RPL_MONONLINE);
			ReplyBuilder offline(user, RPL_MONOFFLINE);

			for (const auto* entry : manager.GetWatched(user))
			{
				ReplyBuilder& out = (ServerInstance->Users.FindNick(entry->GetNick(), true) ? online : offline);
				out.Add(entry->GetNick());
			}

			online.Flush();
			offline.Flush();
		}
		else
			return CmdResult::FAILURE;

		return CmdResult::SUCCESS;
	}
};

class ModuleMonitor final
	: public Module
	, public ISupport::EventListener
	, public Monitor::APIBase
{
private:
	IRCv3::Monitor::Manager manager;
	CommandMonitor cmd;
	Cap::Capability extendedcap;

	void SendAlert(unsigned int numeric, const std::string& nick)
	{
		const IRCv3::Monitor::WatcherList* list = manager.GetWatcherList(nick);
		if (!list)
			return;

		for (const auto& curr : *list)
			curr->WriteNumeric(numeric, nick);
	}

public:
	ModuleMonitor()
		: Module(VF_VENDOR, "Adds the /MONITOR command which allows users to find out when their friends are connected to the server.")
		, ISupport::EventListener(this)
		, Monitor::APIBase(this)
		, manager(this, "monitor")
		, cmd(this, manager)
		, extendedcap(this, "extended-monitor")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("monitor");
		cmd.maxmonitor = tag->getNum<unsigned long>("maxentries", 30, 1);
	}

	void OnPostConnect(User* user) override
	{
		SendAlert(RPL_MONONLINE, user->nick);
	}

	void OnUserPostNick(User* user, const std::string& oldnick) override
	{
		// Detect and ignore nickname case change
		if (ServerInstance->Users.FindNick(oldnick) == user)
			return;

		SendAlert(RPL_MONOFFLINE, oldnick);
		SendAlert(RPL_MONONLINE, user->nick);
	}

	void OnUserQuit(User* user, const std::string& message, const std::string& oper_message) override
	{
		LocalUser* localuser = IS_LOCAL(user);
		if (localuser)
			manager.UnwatchAll(localuser);
		SendAlert(RPL_MONOFFLINE, user->nick);
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["MONITOR"] = ConvToStr(cmd.maxmonitor);
	}

	void ForEachWatcher(User* user, Monitor::ForEachHandler& handler, bool extended_only) override
	{
		const IRCv3::Monitor::WatcherList* list = manager.GetWatcherList(user->nick);
		if (!list)
			return;

		for (IRCv3::Monitor::WatcherList::const_iterator i = list->begin(); i != list->end(); ++i)
		{
			LocalUser* curr = *i;
			if (!extended_only || extendedcap.IsEnabled(curr))
				handler.Execute(curr);
		}
	}
};

MODULE_INIT(ModuleMonitor)

#endif
