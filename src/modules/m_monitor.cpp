/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

namespace IRCv3
{
	namespace Monitor
	{
		class ExtItem;
		struct Entry;
		class Manager;
		class ManagerInternal;

		typedef std::vector<Entry*> WatchedList;
		typedef std::vector<LocalUser*> WatcherList;
	}
}

struct IRCv3::Monitor::Entry
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

class IRCv3::Monitor::Manager
{
	struct ExtData
	{
		WatchedList list;
	};

	class ExtItem : public ExtensionItem
	{
		Manager& manager;

	 public:
		ExtItem(Module* mod, const std::string& extname, Manager& managerref)
			: ExtensionItem(extname, ExtensionItem::EXT_USER, mod)
			, manager(managerref)
		{
		}

		ExtData* get(Extensible* container, bool create = false)
		{
			ExtData* extdata = static_cast<ExtData*>(get_raw(container));
			if ((!extdata) && (create))
			{
				extdata = new ExtData;
				set_raw(container, extdata);
			}
			return extdata;
		}

		void unset(Extensible* container)
		{
			free(unset_raw(container));
		}

		std::string serialize(SerializeFormat format, const Extensible* container, void* item) const
		{
			std::string ret;
			if (format == FORMAT_NETWORK)
				return ret;

			const ExtData* extdata = static_cast<ExtData*>(item);
			for (WatchedList::const_iterator i = extdata->list.begin(); i != extdata->list.end(); ++i)
			{
				const Entry* entry = *i;
				ret.append(entry->GetNick()).push_back(' ');
			}
			if (!ret.empty())
				ret.erase(ret.size()-1);
			return ret;
		}

		void unserialize(SerializeFormat format, Extensible* container, const std::string& value);

		void free(void* item)
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

	WatchResult Watch(LocalUser* user, const std::string& nick, unsigned int maxwatch)
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
			ext.unset(user);
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
		ext.unset(user);
	}

	WatcherList* GetWatcherList(const std::string& nick)
	{
		Entry* entry = Find(nick);
		if (entry)
			return &entry->watchers;
		return NULL;
	}

	static User* FindNick(const std::string& nick)
	{
		User* user = ServerInstance->FindNickOnly(nick);
		if ((user) && (user->registered == REG_ALL))
			return user;
		return NULL;
	}

 private:
 	typedef TR1NS::unordered_map<std::string, Entry, irc::insensitive, irc::StrHashComp> NickHash;

	Entry* Find(const std::string& nick)
	{
		NickHash::iterator it = nicks.find(nick);
		if (it != nicks.end())
			return &it->second;
		return NULL;
	}

	Entry* AddWatcher(const std::string& nick, LocalUser* user)
	{
		std::pair<NickHash::iterator, bool> ret = nicks.insert(std::make_pair(nick, Entry()));
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
		ExtData* extdata = ext.get(user, create);
		if (!extdata)
			return NULL;
		return &extdata->list;
	}

 	NickHash nicks;
 	ExtItem ext;
 	WatchedList emptywatchedlist;
};

// inline is needed in static builds to support m_watch including the Manager code from this file
inline void IRCv3::Monitor::Manager::ExtItem::unserialize(SerializeFormat format, Extensible* container, const std::string& value)
{
	if (format == FORMAT_NETWORK)
		return;

	irc::spacesepstream ss(value);
	for (std::string nick; ss.GetToken(nick); )
		manager.Watch(static_cast<LocalUser*>(container), nick, UINT_MAX);
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

class CommandMonitor : public SplitCommand
{
	typedef Numeric::Builder<> ReplyBuilder;
	// Additional penalty for the /MONITOR L and /MONITOR S commands that request a list from the server
	static const unsigned int ListPenalty = 3000;

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
				user->WriteNumeric(ERR_MONLISTFULL, maxmonitor, InspIRCd::Format("%s%s%s", nick.c_str(), (ss.StreamEnd() ? "" : ","), ss.GetRemaining().c_str()), "Monitor list is full");
				break;
			}
			else if (result != IRCv3::Monitor::Manager::WR_OK)
				continue; // Already added or invalid nick

			ReplyBuilder& out = (IRCv3::Monitor::Manager::FindNick(nick) ? online : offline);
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
	unsigned int maxmonitor;

	CommandMonitor(Module* mod, IRCv3::Monitor::Manager& managerref)
		: SplitCommand(mod, "MONITOR", 1)
		, manager(managerref)
	{
		Penalty = 2;
		allow_empty_last_param = false;
		syntax = "[C|L|S|+ <nick1>[,<nick2>]|- <nick1>[,<nick2>]";
	}

	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser* user)
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
			for (IRCv3::Monitor::WatchedList::const_iterator i = list.begin(); i != list.end(); ++i)
			{
				IRCv3::Monitor::Entry* entry = *i;
				out.Add(entry->GetNick());
			}
			out.Flush();
			user->WriteNumeric(RPL_ENDOFMONLIST, "End of MONITOR list");
		}
		else if (subcmd == 'S')
		{
			user->CommandFloodPenalty += ListPenalty;

			ReplyBuilder online(user, RPL_MONONLINE);
			ReplyBuilder offline(user, RPL_MONOFFLINE);

			const IRCv3::Monitor::WatchedList& list = manager.GetWatched(user);
			for (IRCv3::Monitor::WatchedList::const_iterator i = list.begin(); i != list.end(); ++i)
			{
				IRCv3::Monitor::Entry* entry = *i;
				ReplyBuilder& out = (IRCv3::Monitor::Manager::FindNick(entry->GetNick()) ? online : offline);
				out.Add(entry->GetNick());
			}

			online.Flush();
			offline.Flush();
		}
		else
			return CMD_FAILURE;

		return CMD_SUCCESS;
	}
};

class ModuleMonitor : public Module
{
	IRCv3::Monitor::Manager manager;
	CommandMonitor cmd;

	void SendAlert(unsigned int numeric, const std::string& nick)
	{
		const IRCv3::Monitor::WatcherList* list = manager.GetWatcherList(nick);
		if (!list)
			return;

		for (IRCv3::Monitor::WatcherList::const_iterator i = list->begin(); i != list->end(); ++i)
		{
			LocalUser* curr = *i;
			curr->WriteNumeric(numeric, nick);
		}
	}

 public:
	ModuleMonitor()
		: manager(this, "monitor")
		, cmd(this, manager)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("monitor");
		cmd.maxmonitor = tag->getInt("maxentries", 30, 1);
	}

	void OnPostConnect(User* user) CXX11_OVERRIDE
	{
		SendAlert(RPL_MONONLINE, user->nick);
	}

	void OnUserPostNick(User* user, const std::string& oldnick) CXX11_OVERRIDE
	{
		// Detect and ignore nickname case change
		if (ServerInstance->FindNickOnly(oldnick) == user)
			return;

		SendAlert(RPL_MONOFFLINE, oldnick);
		SendAlert(RPL_MONONLINE, user->nick);
	}

	void OnUserQuit(User* user, const std::string& message, const std::string& oper_message) CXX11_OVERRIDE
	{
		LocalUser* localuser = IS_LOCAL(user);
		if (localuser)
			manager.UnwatchAll(localuser);
		SendAlert(RPL_MONOFFLINE, user->nick);
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["MONITOR"] = ConvToStr(cmd.maxmonitor);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides MONITOR support", VF_VENDOR);
	}
};

MODULE_INIT(ModuleMonitor)

#endif
