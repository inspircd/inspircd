/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 Val Lorentz <progval+git@progval.net>
 *   Copyright (C) 2013, 2017-2018, 2020-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Dennis Friis <peavey@inspircd.org>
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
#include "modules/stats.h"
#include "modules/whois.h"
#include "timeutils.h"

enum
{
	// From RFC 1459.
	RPL_WHOWASUSER = 314,
	RPL_ENDOFWHOWAS = 369,
	ERR_WASNOSUCHNICK = 406,

	// InspIRCd-specific.
	RPL_WHOWASIP = 652
};

namespace WhoWas
{
	/** One entry for a nick. There may be multiple entries for a nick. */
	struct Entry final
	{
		/** Real host */
		const std::string host;

		/** Displayed host */
		const std::string dhost;

		/** Ident */
		const std::string ident;

		/** Server name */
		const std::string server;

		/** Real name */
		const std::string real;

		/** Signon time */
		const time_t signon;

		/** Initialize this Entry with a user */
		Entry(User* user);
	};

	/** Everything known about one nick */
	struct Nick final
		: public insp::intrusive_list_node<Nick>
	{
		/** A group of users related by nickname */
		typedef std::deque<Entry*> List;

		/** Container where each element has information about one occurrence of this nick */
		List entries;

		/** Time this nick was added to the database */
		const time_t addtime;

		/** Nickname whose information is stored in this class */
		const std::string nick;

		/** Constructor to initialize fields */
		Nick(const std::string& nickname);

		/** Destructor, deallocates all elements in the entries container */
		~Nick();
	};

	class Manager final
	{
	public:
		struct Stats final
		{
			/** Number of currently existing WhoWas::Entry objects */
			size_t entrycount;
		};

		/** Add a user to the whowas database. Called when a user quits.
		 * @param user The user to add to the database
		 */
		void Add(User* user);

		/** Retrieves statistics about the whowas database
		 * @return Whowas statistics as a WhoWas::Manager::Stats struct
		 */
		Stats GetStats() const;

		/** Expires old entries */
		void Maintain();

		/** Updates the current configuration which may result in the database being pruned if the
		 * new values are lower than the current ones.
		 * @param NewGroupSize Maximum number of nicks allowed in the database. In case there are this many nicks
		 * in the database and one more is added, the oldest one is removed (FIFO).
		 * @param NewMaxGroups Maximum number of entries per nick
		 * @param NewMaxKeep Seconds how long each nick should be kept
		 */
		void UpdateConfig(unsigned int NewGroupSize, unsigned int NewMaxGroups, unsigned int NewMaxKeep);

		/** Retrieves all data known about a given nick
		 * @param nick Nickname to find, case insensitive (IRC casemapping)
		 * @return A pointer to a WhoWas::Nick if the nick was found, NULL otherwise
		 */
		const Nick* FindNick(const std::string& nick) const;

		/** Returns true if WHOWAS is enabled according to the current configuration
		 * @return True if WHOWAS is enabled according to the configuration, false if WHOWAS is disabled
		 */
		bool IsEnabled() const;

		/** Destructor */
		~Manager();

	private:
		/** Order in which the users were added into the map, used to remove oldest nick */
		typedef insp::intrusive_list_tail<Nick> FIFO;

		/** Sets of users in the whowas system */
		typedef std::unordered_map<std::string, WhoWas::Nick*, irc::insensitive, irc::StrHashComp> whowas_users;

		/** Primary container, links nicknames tracked by WHOWAS to a list of records */
		whowas_users whowas;

		/** List of nicknames in the order they were inserted into the map */
		FIFO whowas_fifo;

		/** Max number of WhoWas entries per user. */
		unsigned int GroupSize = 0;

		/** Max number of cumulative user-entries in WhoWas.
		 * When max reached and added to, push out oldest entry FIFO style.
		 */
		unsigned int MaxGroups = 0;

		/** Max seconds a user is kept in WhoWas before being pruned. */
		unsigned int MaxKeep = 0;

		/** Shrink all data structures to honor the current settings */
		void Prune();

		/** Remove a nick (and all entries belonging to it) from the database
		 * @param it Iterator to the nick to purge
		 */
		void PurgeNick(whowas_users::iterator it);

		/** Remove a nick (and all entries belonging to it) from the database
		 * @param nick Nick to purge
		 */
		void PurgeNick(WhoWas::Nick* nick);
	};
}

class CommandWhowas final
	: public Command
{
public:
	// Manager handling all whowas database related tasks
	WhoWas::Manager manager;

	CommandWhowas(Module* parent);
	CmdResult Handle(User* user, const Params& parameters) override;
};

CommandWhowas::CommandWhowas( Module* parent)
	: Command(parent, "WHOWAS", 1)
{
	allow_empty_last_param = true;
	penalty = 2000;
	syntax = { "<nick> [<count>]" };
}

CmdResult CommandWhowas::Handle(User* user, const Params& parameters)
{
	/* if whowas disabled in config */
	if (!manager.IsEnabled())
	{
		user->WriteNumeric(ERR_UNKNOWNCOMMAND, name, "This command has been disabled.");
		return CmdResult::FAILURE;
	}

	if (parameters[0].empty())
	{
		user->WriteNumeric(ERR_NONICKNAMEGIVEN, "No nickname given");
		return CmdResult::FAILURE;
	}

	const WhoWas::Nick* const nick = manager.FindNick(parameters[0]);
	if (!nick)
	{
		user->WriteNumeric(ERR_WASNOSUCHNICK, parameters[0], "There was no such nickname");
	}
	else
	{
		WhoWas::Nick::List::const_reverse_iterator last = nick->entries.rend();
		if (parameters.size() > 1)
		{
			size_t count = ConvToNum<size_t>(parameters[1]);
			if (count > 0 && (size_t) std::distance(nick->entries.rbegin(), last) > count)
				last = nick->entries.rbegin() + count;
		}

		for (const auto* u : insp::iterator_range(nick->entries.rbegin(), last))
		{
			user->WriteNumeric(RPL_WHOWASUSER, parameters[0], u->ident, u->dhost, '*', u->real);

			if (user->HasPrivPermission("users/auspex"))
				user->WriteNumeric(RPL_WHOWASIP, parameters[0], INSP_FORMAT("was connecting from *@{}", u->host));

			const std::string signon = Time::ToString(u->signon);
			bool hide_server = (!ServerInstance->Config->HideServer.empty() && !user->HasPrivPermission("servers/auspex"));
			user->WriteNumeric(RPL_WHOISSERVER, parameters[0], (hide_server ? ServerInstance->Config->HideServer : u->server), signon);
		}
	}

	user->WriteNumeric(RPL_ENDOFWHOWAS, parameters[0], "End of WHOWAS");
	return CmdResult::SUCCESS;
}

const WhoWas::Nick* WhoWas::Manager::FindNick(const std::string& nickname) const
{
	whowas_users::const_iterator it = whowas.find(nickname);
	if (it == whowas.end())
		return nullptr;
	return it->second;
}

WhoWas::Manager::Stats WhoWas::Manager::GetStats() const
{
	size_t entrycount = 0;
	for (const auto& [_, nick] : whowas)
		entrycount += nick->entries.size();

	Stats stats;
	stats.entrycount = entrycount;
	return stats;
}

void WhoWas::Manager::Add(User* user)
{
	if (!IsEnabled())
		return;

	// Insert nick if it doesn't exist
	// 'first' will point to the newly inserted element or to the existing element with an equivalent key
	std::pair<whowas_users::iterator, bool> ret = whowas.emplace(user->nick, nullptr);

	if (ret.second) // If inserted
	{
		// This nick is new, create a list for it and add the first record to it
		auto* nick = new WhoWas::Nick(ret.first->first);
		nick->entries.push_back(new Entry(user));
		ret.first->second = nick;

		// Add this nick to the fifo too
		whowas_fifo.push_back(nick);

		if (whowas.size() > this->MaxGroups)
		{
			// Too many nicks, remove the nick which was inserted the longest time ago from both the map and the fifo
			PurgeNick(whowas_fifo.front());
		}
	}
	else
	{
		// We've met this nick before, add a new record to the list
		WhoWas::Nick::List& list = ret.first->second->entries;
		list.push_back(new Entry(user));

		// If there are too many records for this nick, remove the oldest (front)
		if (list.size() > this->GroupSize)
		{
			delete list.front();
			list.pop_front();
		}
	}
}

/* on rehash, refactor maps according to new conf values */
void WhoWas::Manager::Prune()
{
	time_t min = ServerInstance->Time() - this->MaxKeep;

	/* first cut the list to new size (maxgroups) and also prune entries that are timed out. */
	while (!whowas_fifo.empty())
	{
		WhoWas::Nick* nick = whowas_fifo.front();
		if ((whowas_fifo.size() > this->MaxGroups) || (nick->addtime < min))
			PurgeNick(nick);
		else
			break;
	}

	/* Then cut the whowas sets to new size (groupsize) */
	for (whowas_users::iterator i = whowas.begin(); i != whowas.end(); )
	{
		WhoWas::Nick::List& list = i->second->entries;
		while (list.size() > this->GroupSize)
		{
			delete list.front();
			list.pop_front();
		}

		if (list.empty())
			PurgeNick(i++);
		else
			++i;
	}
}

/* call maintain once an hour to remove expired nicks */
void WhoWas::Manager::Maintain()
{
	time_t min = ServerInstance->Time() - this->MaxKeep;
	for (whowas_users::iterator i = whowas.begin(); i != whowas.end(); )
	{
		WhoWas::Nick::List& list = i->second->entries;
		while (!list.empty() && list.front()->signon < min)
		{
			delete list.front();
			list.pop_front();
		}

		if (list.empty())
			PurgeNick(i++);
		else
			++i;
	}
}

WhoWas::Manager::~Manager()
{
	for (const auto& [_, nick] : whowas)
		delete nick;
}

bool WhoWas::Manager::IsEnabled() const
{
	return ((GroupSize != 0) && (MaxGroups != 0));
}

void WhoWas::Manager::UpdateConfig(unsigned int NewGroupSize, unsigned int NewMaxGroups, unsigned int NewMaxKeep)
{
	if ((NewGroupSize == GroupSize) && (NewMaxGroups == MaxGroups) && (NewMaxKeep == MaxKeep))
		return;

	GroupSize = NewGroupSize;
	MaxGroups = NewMaxGroups;
	MaxKeep = NewMaxKeep;
	Prune();
}

void WhoWas::Manager::PurgeNick(whowas_users::iterator it)
{
	WhoWas::Nick* nick = it->second;
	whowas_fifo.erase(nick);
	whowas.erase(it);
	delete nick;
}

void WhoWas::Manager::PurgeNick(WhoWas::Nick* nick)
{
	whowas_users::iterator it = whowas.find(nick->nick);
	if (it == whowas.end())
	{
		ServerInstance->Logs.Debug(MODNAME, "BUG: Inconsistency detected in whowas database, please report");
		return;
	}
	PurgeNick(it);
}

WhoWas::Entry::Entry(User* user)
	: host(user->GetRealHost())
	, dhost(user->GetDisplayedHost())
	, ident(user->ident)
	, server(user->server->GetPublicName())
	, real(user->GetRealName())
	, signon(user->signon)
{
}

WhoWas::Nick::Nick(const std::string& nickname)
	: addtime(ServerInstance->Time())
	, nick(nickname)
{
}

WhoWas::Nick::~Nick()
{
	stdalgo::delete_all(entries);
}

class ModuleWhoWas final
	: public Module
	, public Stats::EventListener
{
	CommandWhowas cmd;

public:
	ModuleWhoWas()
		: Module(VF_CORE | VF_VENDOR, "Provides the WHOWAS command")
		, Stats::EventListener(this)
		, cmd(this)
	{
	}

	void OnGarbageCollect() override
	{
		// Remove all entries older than MaxKeep
		cmd.manager.Maintain();
	}

	void OnUserQuit(User* user, const std::string& message, const std::string& oper_message) override
	{
		cmd.manager.Add(user);
	}

	ModResult OnStats(Stats::Context& stats) override
	{
		if (stats.GetSymbol() == 'z')
			stats.AddRow(249, "Whowas entries: "+ConvToStr(cmd.manager.GetStats().entrycount));

		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("whowas");
		unsigned int NewGroupSize = tag->getNum<unsigned int>("groupsize", 10, 0, 10000);
		unsigned int NewMaxGroups = tag->getNum<unsigned int>("maxgroups", 10240, 0, 1000000);
		unsigned int NewMaxKeep = static_cast<unsigned int>(tag->getDuration("maxkeep", 3600, 3600));

		cmd.manager.UpdateConfig(NewGroupSize, NewMaxGroups, NewMaxKeep);
	}
};

MODULE_INIT(ModuleWhoWas)
