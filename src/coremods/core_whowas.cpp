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
		user->WriteNumeric(ERR_UNKNOWNCOMMAND, "%s :This command has been disabled.", name.c_str());
		return CMD_FAILURE;
	}

	const WhoWas::Nick* const nick = manager.FindNick(parameters[0]);
	if (!nick)
	{
		user->WriteNumeric(ERR_WASNOSUCHNICK, "%s :There was no such nickname", parameters[0].c_str());
	}
	else
	{
		const WhoWas::Nick::List& list = nick->entries;
		if (!list.empty())
		{
			for (WhoWas::Nick::List::const_iterator i = list.begin(); i != list.end(); ++i)
			{
				WhoWas::Entry* u = *i;

				user->WriteNumeric(RPL_WHOWASUSER, "%s %s %s * :%s", parameters[0].c_str(),
					u->ident.c_str(),u->dhost.c_str(),u->gecos.c_str());

				if (user->HasPrivPermission("users/auspex"))
					user->WriteNumeric(RPL_WHOWASIP, "%s :was connecting from *@%s",
						parameters[0].c_str(), u->host.c_str());

				std::string signon = InspIRCd::TimeString(u->signon);
				bool hide_server = (!ServerInstance->Config->HideWhoisServer.empty() && !user->HasPrivPermission("servers/auspex"));
				user->WriteNumeric(RPL_WHOISSERVER, "%s %s :%s", parameters[0].c_str(), (hide_server ? ServerInstance->Config->HideWhoisServer.c_str() : u->server.c_str()), signon.c_str());
			}
		}
	}

	user->WriteNumeric(RPL_ENDOFWHOWAS, "%s :End of WHOWAS", parameters[0].c_str());
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

	const Nick* nick = it->second;
	if (nick->entries.empty())
		return NULL;
	return nick;
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
			nick = whowas_fifo.front();
			whowas_fifo.pop_front();
			whowas.erase(nick->nick);
			delete nick;
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
		{
			/* hopefully redundant integrity check, but added while debugging r6216 */
			if (!whowas.erase(nick->nick))
			{
				/* this should never happen, if it does maps are corrupt */
				ServerInstance->Logs->Log("WHOWAS", LOG_DEFAULT, "BUG: Whowas maps got corrupted! (1)");
				return;
			}

			whowas_fifo.pop_front();
			delete nick;
		}
		else
			break;
	}

	/* Then cut the whowas sets to new size (groupsize) */
	for (whowas_users::iterator i = whowas.begin(); i != whowas.end(); ++i)
	{
		WhoWas::Nick::List& list = i->second->entries;
		while (list.size() > this->GroupSize)
		{
			delete list.front();
			list.pop_front();
		}
	}
}

/* call maintain once an hour to remove expired nicks */
void WhoWas::Manager::Maintain()
{
	time_t min = ServerInstance->Time() - this->MaxKeep;
	for (whowas_users::iterator i = whowas.begin(); i != whowas.end(); ++i)
	{
		WhoWas::Nick::List& list = i->second->entries;
		while (!list.empty() && list.front()->signon < min)
		{
			delete list.front();
			list.pop_front();
		}
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

	void OnGarbageCollect()
	{
		// Remove all entries older than MaxKeep
		cmd.manager.Maintain();
	}

	void OnUserQuit(User* user, const std::string& message, const std::string& oper_message)
	{
		cmd.manager.Add(user);
	}

	ModResult OnStats(char symbol, User* user, string_list &results)
	{
		if (symbol == 'z')
			results.push_back("249 "+user->nick+" :Whowas entries: "+ConvToStr(cmd.manager.GetStats().entrycount));

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

	Version GetVersion()
	{
		return Version("WHOWAS", VF_VENDOR);
	}
};

MODULE_INIT(ModuleWhoWas)
