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
	, GroupSize(0), MaxGroups(0), MaxKeep(0)
{
	syntax = "<nick>{,<nick>}";
	Penalty = 2;
}

CmdResult CommandWhowas::Handle (const std::vector<std::string>& parameters, User* user)
{
	/* if whowas disabled in config */
	if (this->GroupSize == 0 || this->MaxGroups == 0)
	{
		user->WriteNumeric(ERR_UNKNOWNCOMMAND, "%s :This command has been disabled.", name.c_str());
		return CMD_FAILURE;
	}

	whowas_users::iterator i = whowas.find(assign(parameters[0]));

	if (i == whowas.end())
	{
		user->WriteNumeric(ERR_WASNOSUCHNICK, "%s :There was no such nickname", parameters[0].c_str());
	}
	else
	{
		whowas_set* grp = i->second;
		if (!grp->empty())
		{
			for (whowas_set::iterator ux = grp->begin(); ux != grp->end(); ux++)
			{
				WhoWasGroup* u = *ux;

				user->WriteNumeric(RPL_WHOWASUSER, "%s %s %s * :%s", parameters[0].c_str(),
					u->ident.c_str(),u->dhost.c_str(),u->gecos.c_str());

				if (user->HasPrivPermission("users/auspex"))
					user->WriteNumeric(RPL_WHOWASIP, "%s :was connecting from *@%s",
						parameters[0].c_str(), u->host.c_str());

				std::string signon = ServerInstance->TimeString(u->signon);
				bool hide_server = (!ServerInstance->Config->HideWhoisServer.empty() && !user->HasPrivPermission("servers/auspex"));
				user->WriteNumeric(RPL_WHOISSERVER, "%s %s :%s", parameters[0].c_str(), (hide_server ? ServerInstance->Config->HideWhoisServer.c_str() : u->server.c_str()), signon.c_str());
			}
		}
		else
		{
			user->WriteNumeric(ERR_WASNOSUCHNICK, "%s :There was no such nickname", parameters[0].c_str());
		}
	}

	user->WriteNumeric(RPL_ENDOFWHOWAS, "%s :End of WHOWAS", parameters[0].c_str());
	return CMD_SUCCESS;
}

std::string CommandWhowas::GetStats()
{
	int whowas_size = 0;
	int whowas_bytes = 0;
	for (whowas_users::iterator i = whowas.begin(); i != whowas.end(); ++i)
	{
		whowas_set* n = i->second;
		whowas_size += n->size();
		whowas_bytes += (sizeof(whowas_set) + ( sizeof(WhoWasGroup) * n->size() ) );
	}
	return "Whowas entries: " +ConvToStr(whowas_size)+" ("+ConvToStr(whowas_bytes)+" bytes)";
}

void CommandWhowas::AddToWhoWas(User* user)
{
	/* if whowas disabled */
	if (this->GroupSize == 0 || this->MaxGroups == 0)
	{
		return;
	}

	// Insert nick if it doesn't exist
	// 'first' will point to the newly inserted element or to the existing element with an equivalent key
	std::pair<whowas_users::iterator, bool> ret = whowas.insert(std::make_pair(irc::string(user->nick.c_str()), static_cast<whowas_set*>(NULL)));

	if (ret.second) // If inserted
	{
		// This nick is new, create a list for it and add the first record to it
		whowas_set* n = new whowas_set;
		n->push_back(new WhoWasGroup(user));
		ret.first->second = n;

		// Add this nick to the fifo too
		whowas_fifo.push_back(std::make_pair(ServerInstance->Time(), ret.first->first));

		if (whowas.size() > this->MaxGroups)
		{
			// Too many nicks, remove the nick which was inserted the longest time ago from both the map and the fifo
			whowas_users::iterator it = whowas.find(whowas_fifo.front().second);
			if (it != whowas.end())
			{
				whowas_set* set = it->second;
				for (whowas_set::iterator i = set->begin(); i != set->end(); ++i)
					delete *i;

				delete set;
				whowas.erase(it);
			}
			whowas_fifo.pop_front();
		}
	}
	else
	{
		// We've met this nick before, add a new record to the list
		whowas_set* set = ret.first->second;
		set->push_back(new WhoWasGroup(user));

		// If there are too many records for this nick, remove the oldest (front)
		if (set->size() > this->GroupSize)
		{
			delete set->front();
			set->pop_front();
		}
	}
}

/* on rehash, refactor maps according to new conf values */
void CommandWhowas::Prune()
{
	time_t min = ServerInstance->Time() - this->MaxKeep;

	/* first cut the list to new size (maxgroups) and also prune entries that are timed out. */
	while (!whowas_fifo.empty())
	{
		if ((whowas_fifo.size() > this->MaxGroups) || (whowas_fifo.front().first < min))
		{
			whowas_users::iterator iter = whowas.find(whowas_fifo.front().second);

			/* hopefully redundant integrity check, but added while debugging r6216 */
			if (iter == whowas.end())
			{
				/* this should never happen, if it does maps are corrupt */
				ServerInstance->Logs->Log("WHOWAS", LOG_DEFAULT, "BUG: Whowas maps got corrupted! (1)");
				return;
			}

			whowas_set* set = iter->second;
			for (whowas_set::iterator i = set->begin(); i != set->end(); ++i)
				delete *i;

			delete set;
			whowas.erase(iter);
			whowas_fifo.pop_front();
		}
		else
			break;
	}

	/* Then cut the whowas sets to new size (groupsize) */
	for (whowas_users::iterator i = whowas.begin(); i != whowas.end(); ++i)
	{
		whowas_set* n = i->second;
		while (n->size() > this->GroupSize)
		{
			delete n->front();
			n->pop_front();
		}
	}
}

/* call maintain once an hour to remove expired nicks */
void CommandWhowas::Maintain()
{
	time_t min = ServerInstance->Time() - this->MaxKeep;
	for (whowas_users::iterator i = whowas.begin(); i != whowas.end(); ++i)
	{
		whowas_set* set = i->second;
		while (!set->empty() && set->front()->signon < min)
		{
			delete set->front();
			set->pop_front();
		}
	}
}

CommandWhowas::~CommandWhowas()
{
	for (whowas_users::iterator i = whowas.begin(); i != whowas.end(); ++i)
	{
		whowas_set* set = i->second;
		for (whowas_set::iterator j = set->begin(); j != set->end(); ++j)
			delete *j;

		delete set;
	}
}

WhoWasGroup::WhoWasGroup(User* user) : host(user->host), dhost(user->dhost), ident(user->ident),
	server(user->server), gecos(user->fullname), signon(user->signon)
{
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
		cmd.Maintain();
	}

	void OnUserQuit(User* user, const std::string& message, const std::string& oper_message)
	{
		cmd.AddToWhoWas(user);
	}

	ModResult OnStats(char symbol, User* user, string_list &results)
	{
		if (symbol == 'z')
			results.push_back(ServerInstance->Config->ServerName+" 249 "+user->nick+" :"+cmd.GetStats());

		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("whowas");
		unsigned int NewGroupSize = tag->getInt("groupsize", 10, 0, 10000);
		unsigned int NewMaxGroups = tag->getInt("maxgroups", 10240, 0, 1000000);
		unsigned int NewMaxKeep = tag->getDuration("maxkeep", 3600, 3600);

		if ((NewGroupSize == cmd.GroupSize) && (NewMaxGroups == cmd.MaxGroups) && (NewMaxKeep == cmd.MaxKeep))
			return;

		cmd.GroupSize = NewGroupSize;
		cmd.MaxGroups = NewMaxGroups;
		cmd.MaxKeep = NewMaxKeep;
		cmd.Prune();
	}

	Version GetVersion()
	{
		return Version("WHOWAS", VF_VENDOR);
	}
};

MODULE_INIT(ModuleWhoWas)
