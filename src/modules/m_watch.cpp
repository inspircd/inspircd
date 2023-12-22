/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2017-2018, 2023 Sadie Powell <sadie@witchery.services>
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
#include "modules/away.h"
#include "modules/isupport.h"

#define INSPIRCD_MONITOR_MANAGER_ONLY
#include "m_monitor.cpp"

enum
{
	RPL_REAWAY = 597,
	RPL_GONEAWAY = 598,
	RPL_NOTAWAY = 599,
	RPL_LOGON = 600,
	RPL_LOGOFF = 601,
	RPL_WATCHOFF = 602,
	RPL_WATCHSTAT = 603,
	RPL_NOWON = 604,
	RPL_NOWOFF = 605,
	RPL_WATCHLIST = 606,
	RPL_ENDOFWATCHLIST = 607,
	// RPL_CLEARWATCH = 608, // unused
	RPL_NOWISAWAY = 609,
	ERR_TOOMANYWATCH = 512,
	ERR_INVALIDWATCHNICK = 942
};

class CommandWatch final
	: public SplitCommand
{
	// Additional penalty for /WATCH commands that request a list from the server
	static constexpr unsigned int ListPenalty = 4000;

	IRCv3::Monitor::Manager& manager;

	static void SendOnlineOffline(LocalUser* user, const std::string& nick, bool show_offline = true)
	{
		User* target = ServerInstance->Users.FindNick(nick, true);
		if (target)
		{
			// The away state should only be sent if the client requests away notifications for a nick but 2.0 always sends them so we do that too
			if (target->IsAway())
				user->WriteNumeric(RPL_NOWISAWAY, target->nick, target->GetDisplayedHost(), target->GetDisplayedHost(), target->away->time, "is away");
			else
				user->WriteNumeric(RPL_NOWON, target->nick, target->GetDisplayedHost(), target->GetDisplayedHost(), target->nickchanged, "is online");
		}
		else if (show_offline)
			user->WriteNumeric(RPL_NOWOFF, nick, "*", "*", "0", "is offline");
	}

	void HandlePlus(LocalUser* user, const std::string& nick)
	{
		IRCv3::Monitor::Manager::WatchResult result = manager.Watch(user, nick, maxwatch);
		if (result == IRCv3::Monitor::Manager::WR_TOOMANY)
		{
			// List is full, send error numeric
			user->WriteNumeric(ERR_TOOMANYWATCH, nick, "Too many WATCH entries");
			return;
		}
		else if (result == IRCv3::Monitor::Manager::WR_INVALIDNICK)
		{
			user->WriteNumeric(ERR_INVALIDWATCHNICK, nick, "Invalid nickname");
			return;
		}
		else if (result != IRCv3::Monitor::Manager::WR_OK)
			return;

		SendOnlineOffline(user, nick);
	}

	void HandleMinus(LocalUser* user, const std::string& nick)
	{
		if (!manager.Unwatch(user, nick))
			return;

		User* target = ServerInstance->Users.FindNick(nick, true);
		if (target)
			user->WriteNumeric(RPL_WATCHOFF, target->nick, target->GetDisplayedHost(), target->GetDisplayedHost(), target->nickchanged, "stopped watching");
		else
			user->WriteNumeric(RPL_WATCHOFF, nick, "*", "*", "0", "stopped watching");
	}

	void HandleList(LocalUser* user, bool show_offline)
	{
		user->CommandFloodPenalty += ListPenalty;
		for (const auto* entry : manager.GetWatched(user))
			SendOnlineOffline(user, entry->GetNick(), show_offline);
		user->WriteNumeric(RPL_ENDOFWATCHLIST, "End of WATCH list");
	}

	void HandleStats(LocalUser* user)
	{
		user->CommandFloodPenalty += ListPenalty;

		// Do not show how many clients are watching this nick, it's pointless
		const IRCv3::Monitor::WatchedList& list = manager.GetWatched(user);
		user->WriteNumeric(RPL_WATCHSTAT, INSP_FORMAT("You have {} and are on 0 WATCH entries", list.size()));

		Numeric::Builder<' '> out(user, RPL_WATCHLIST);
		for (const auto* entry : list)
			out.Add(entry->GetNick());
		out.Flush();
		user->WriteNumeric(RPL_ENDOFWATCHLIST, "End of WATCH S");
	}

public:
	unsigned long maxwatch;

	CommandWatch(Module* mod, IRCv3::Monitor::Manager& managerref)
		: SplitCommand(mod, "WATCH")
		, manager(managerref)
	{
		syntax = { "C", "L", "l", "S", "(+|-)<nick> [(+|-)<nick>]+" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		if (parameters.empty())
		{
			HandleList(user, false);
			return CmdResult::SUCCESS;
		}

		bool watch_l_done = false;
		bool watch_s_done = false;

		for (const auto& token : parameters)
		{
			char subcmd = toupper(token[0]);
			if (subcmd == '+')
			{
				HandlePlus(user, token.substr(1));
			}
			else if (subcmd == '-')
			{
				HandleMinus(user, token.substr(1));
			}
			else if (subcmd == 'C')
			{
				manager.UnwatchAll(user);
			}
			else if ((subcmd == 'L') && (!watch_l_done))
			{
				watch_l_done = true;
				// WATCH L requests a full list with online and offline nicks
				// WATCH l requests a list with only online nicks
				HandleList(user, (token[0] == 'L'));
			}
			else if ((subcmd == 'S') && (!watch_s_done))
			{
				watch_s_done = true;
				HandleStats(user);
			}
		}
		return CmdResult::SUCCESS;
	}
};

class ModuleWatch final
	: public Module
	, public Away::EventListener
	, public ISupport::EventListener
{
private:
	IRCv3::Monitor::Manager manager;
	CommandWatch cmd;

	void SendAlert(User* user, const std::string& nick, unsigned int numeric, const char* numerictext, time_t shownts)
	{
		const IRCv3::Monitor::WatcherList* list = manager.GetWatcherList(nick);
		if (!list)
			return;

		Numeric::Numeric num(numeric);
		num.push(nick).push(user->GetDisplayedUser()).push(user->GetDisplayedHost()).push(ConvToStr(shownts)).push(numerictext);
		for (const auto& curr : *list)
			curr->WriteNumeric(num);
	}

	void Online(User* user)
	{
		SendAlert(user, user->nick, RPL_LOGON, "arrived online", user->nickchanged);
		if (user->IsAway())
			OnUserAway(user, std::nullopt);
	}

	void Offline(User* user, const std::string& nick)
	{
		SendAlert(user, nick, RPL_LOGOFF, "went offline", user->nickchanged);
	}

public:
	ModuleWatch()
		: Module(VF_VENDOR, "Adds the /WATCH command which allows users to find out when their friends are connected to the server.")
		, Away::EventListener(this)
		, ISupport::EventListener(this)
		, manager(this, "watch")
		, cmd(this, manager)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("watch");
		cmd.maxwatch = tag->getNum<unsigned long>("maxwatch", 30, 1);
	}

	void OnPostConnect(User* user) override
	{
		Online(user);
	}

	void OnUserPostNick(User* user, const std::string& oldnick) override
	{
		// Detect and ignore nickname case change
		if (ServerInstance->Users.FindNick(oldnick) == user)
			return;

		Offline(user, oldnick);
		Online(user);
	}

	void OnUserQuit(User* user, const std::string& message, const std::string& oper_message) override
	{
		LocalUser* localuser = IS_LOCAL(user);
		if (localuser)
			manager.UnwatchAll(localuser);
		Offline(user, user->nick);
	}

	void OnUserAway(User* user, const std::optional<AwayState>& prevstate) override
	{
		unsigned int numeric = prevstate ? RPL_REAWAY : RPL_GONEAWAY;
		SendAlert(user, user->nick, numeric, user->away->message.c_str(), user->away->time);
	}

	void OnUserBack(User* user, const std::optional<AwayState>& prevstate) override
	{
		SendAlert(user, user->nick, RPL_NOTAWAY, "is no longer away", ServerInstance->Time());
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["WATCH"] = ConvToStr(cmd.maxwatch);
	}
};

MODULE_INIT(ModuleWatch)
