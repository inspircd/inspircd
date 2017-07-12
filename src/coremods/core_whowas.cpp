/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
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
#include "commands/cmd_whowas.h"

CommandWhowas::CommandWhowas( Module* parent)
	: Command(parent, "WHOWAS", 1)
{
	syntax = "<nick>{,<nick>}";
	Penalty = 2;
}

CmdResult CommandWhowas::Handle (const std::vector<std::string>& parameters, User* user)
{
	/* if whowas disabled in config */
	if (!manager.IsEnabled())
	{
		user->WriteNumeric(ERR_UNKNOWNCOMMAND, name, "This command has been disabled.");
		return CMD_FAILURE;
	}

	const WhoWas::Nick* const nick = manager.FindNick(parameters[0]);
	if (!nick)
	{
		user->WriteNumeric(ERR_WASNOSUCHNICK, parameters[0], "There was no such nickname");
	}
	else
	{
		const WhoWas::Nick::List& list = nick->entries;
		for (WhoWas::Nick::List::const_iterator i = list.begin(); i != list.end(); ++i)
		{
			WhoWas::Entry* u = *i;

			user->WriteNumeric(RPL_WHOWASUSER, parameters[0], u->ident, u->dhost, '*', u->gecos);

			if (user->HasPrivPermission("users/auspex"))
				user->WriteNumeric(RPL_WHOWASIP, parameters[0], InspIRCd::Format("was connecting from *@%s", u->host.c_str()));

			std::string signon = InspIRCd::TimeString(u->signon);
			bool hide_server = (!ServerInstance->Config->HideWhoisServer.empty() && !user->HasPrivPermission("servers/auspex"));
			user->WriteNumeric(RPL_WHOISSERVER, parameters[0], (hide_server ? ServerInstance->Config->HideWhoisServer : u->server), signon);
		}
	}

	user->WriteNumeric(RPL_ENDOFWHOWAS, parameters[0], "End of WHOWAS");
	return CMD_SUCCESS;
}

WhoWas::Manager::Manager()
	: GroupSize(0), MaxGroups(0), MaxKeep(0)
{
}

const WhoWas::Nick* WhoWas::Manager::FindNick(const std::string& nickname) const
{
	whowas_users::const_iterator it = whowas.find(nickname);
	if (it == whowas.end())
		return NULL;
	return it->second;
}

WhoWas::Manager::Stats WhoWas::Manager::GetStats() const
{
	size_t entrycount = 0;
	for (whowas_users::const_iterator i = whowas.begin(); i != whowas.end(); ++i)
	{
		WhoWas::Nick::List& list = i->second->entries;
		entrycount += list.size();
	}

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
	std::pair<whowas_users::iterator, bool> ret = whowas.insert(std::make_pair(user->nick, static_cast<WhoWas::Nick*>(NULL)));

	if (ret.second) // If inserted
	{
		// This nick is new, create a list for it and add the first record to it
		WhoWas::Nick* nick = new WhoWas::Nick(ret.first->first);
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
	for (whowas_users::iterator i = whowas.begin(); i != whowas.end(); ++i)
	{
		WhoWas::Nick* nick = i->second;
		delete nick;
	}
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
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "ERROR: Inconsistency detected in whowas database, please report");
		return;
	}
	PurgeNick(it);
}

WhoWas::Entry::Entry(User* user)
	: host(user->host)
	, dhost(user->dhost)
	, ident(user->ident)
	, server(user->server->GetName())
	, gecos(user->fullname)
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

class ModuleWhoWas : public Module
{
	CommandWhowas cmd;

 public:
	ModuleWhoWas() : cmd(this)
	{
	}

	void OnGarbageCollect() CXX11_OVERRIDE
	{
		// Remove all entries older than MaxKeep
		cmd.manager.Maintain();
	}

	void OnUserQuit(User* user, const std::string& message, const std::string& oper_message) CXX11_OVERRIDE
	{
		cmd.manager.Add(user);
	}

	ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE
	{
		if (stats.GetSymbol() == 'z')
			stats.AddRow(249, "Whowas entries: "+ConvToStr(cmd.manager.GetStats().entrycount));

		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("whowas");
		unsigned int NewGroupSize = tag->getInt("groupsize", 10, 0, 10000);
		unsigned int NewMaxGroups = tag->getInt("maxgroups", 10240, 0, 1000000);
		unsigned int NewMaxKeep = tag->getDuration("maxkeep", 3600, 3600);

		cmd.manager.UpdateConfig(NewGroupSize, NewMaxGroups, NewMaxKeep);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("WHOWAS", VF_VENDOR);
	}
};

MODULE_INIT(ModuleWhoWas)
