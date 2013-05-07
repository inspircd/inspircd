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
	: Command(parent, "WHOWAS", 1), WhoWasGroupSize(0), WhoWasMaxGroups(0), WhoWasMaxKeep(0)
{
	syntax = "<nick>{,<nick>}";
	Penalty = 2;
}

CmdResult CommandWhowas::Handle (const std::vector<std::string>& parameters, User* user)
{
	/* if whowas disabled in config */
	if (this->WhoWasGroupSize == 0 || this->WhoWasMaxGroups == 0)
	{
		user->WriteNumeric(421, "%s %s :This command has been disabled.",user->nick.c_str(),name.c_str());
		return CMD_FAILURE;
	}

	whowas_users::iterator i = whowas.find(assign(parameters[0]));

	if (i == whowas.end())
	{
		user->WriteNumeric(406, "%s %s :There was no such nickname",user->nick.c_str(),parameters[0].c_str());
		user->WriteNumeric(369, "%s %s :End of WHOWAS",user->nick.c_str(),parameters[0].c_str());
		return CMD_FAILURE;
	}
	else
	{
		whowas_set* grp = i->second;
		if (grp->size())
		{
			for (whowas_set::iterator ux = grp->begin(); ux != grp->end(); ux++)
			{
				WhoWasGroup* u = *ux;
				time_t rawtime = u->signon;
				tm *timeinfo;
				char b[25];

				timeinfo = localtime(&rawtime);

				strncpy(b,asctime(timeinfo),24);
				b[24] = 0;

				user->WriteNumeric(314, "%s %s %s %s * :%s",user->nick.c_str(),parameters[0].c_str(),
					u->ident.c_str(),u->dhost.c_str(),u->gecos.c_str());

				if (user->HasPrivPermission("users/auspex"))
					user->WriteNumeric(379, "%s %s :was connecting from *@%s",
						user->nick.c_str(), parameters[0].c_str(), u->host.c_str());

				if (!ServerInstance->Config->HideWhoisServer.empty() && !user->HasPrivPermission("servers/auspex"))
					user->WriteNumeric(312, "%s %s %s :%s",user->nick.c_str(),parameters[0].c_str(), ServerInstance->Config->HideWhoisServer.c_str(), b);
				else
					user->WriteNumeric(312, "%s %s %s :%s",user->nick.c_str(),parameters[0].c_str(), u->server.c_str(), b);
			}
		}
		else
		{
			user->WriteNumeric(406, "%s %s :There was no such nickname",user->nick.c_str(),parameters[0].c_str());
			user->WriteNumeric(369, "%s %s :End of WHOWAS",user->nick.c_str(),parameters[0].c_str());
			return CMD_FAILURE;
		}
	}

	user->WriteNumeric(369, "%s %s :End of WHOWAS",user->nick.c_str(),parameters[0].c_str());
	return CMD_SUCCESS;
}

std::string CommandWhowas::GetStats()
{
	int whowas_size = 0;
	int whowas_bytes = 0;
	whowas_users_fifo::iterator iter;
	for (iter = whowas_fifo.begin(); iter != whowas_fifo.end(); iter++)
	{
		whowas_set* n = (whowas_set*)whowas.find(iter->second)->second;
		if (n->size())
		{
			whowas_size += n->size();
			whowas_bytes += (sizeof(whowas_set) + ( sizeof(WhoWasGroup) * n->size() ) );
		}
	}
	return "Whowas entries: " +ConvToStr(whowas_size)+" ("+ConvToStr(whowas_bytes)+" bytes)";
}

void CommandWhowas::AddToWhoWas(User* user)
{
	/* if whowas disabled */
	if (this->WhoWasGroupSize == 0 || this->WhoWasMaxGroups == 0)
	{
		return;
	}

	whowas_users::iterator iter = whowas.find(irc::string(user->nick.c_str()));

	if (iter == whowas.end())
	{
		whowas_set* n = new whowas_set;
		WhoWasGroup *a = new WhoWasGroup(user);
		n->push_back(a);
		whowas[user->nick.c_str()] = n;
		whowas_fifo.push_back(std::make_pair(ServerInstance->Time(),user->nick.c_str()));

		if ((int)(whowas.size()) > this->WhoWasMaxGroups)
		{
			whowas_users::iterator iter2 = whowas.find(whowas_fifo[0].second);
			if (iter2 != whowas.end())
			{
				whowas_set* n2 = (whowas_set*)iter2->second;

				if (n2->size())
				{
					while (n2->begin() != n2->end())
					{
						WhoWasGroup *a2 = *(n2->begin());
						delete a2;
						n2->pop_front();
					}
				}

				delete n2;
				whowas.erase(iter2);
			}
			whowas_fifo.pop_front();
		}
	}
	else
	{
		whowas_set* group = (whowas_set*)iter->second;
		WhoWasGroup *a = new WhoWasGroup(user);
		group->push_back(a);

		if ((int)(group->size()) > this->WhoWasGroupSize)
		{
			WhoWasGroup *a2 = (WhoWasGroup*)*(group->begin());
			delete a2;
			group->pop_front();
		}
	}
}

/* on rehash, refactor maps according to new conf values */
void CommandWhowas::PruneWhoWas(time_t t)
{
	/* config values */
	int groupsize = this->WhoWasGroupSize;
	int maxgroups = this->WhoWasMaxGroups;
	int maxkeep =   this->WhoWasMaxKeep;

	/* first cut the list to new size (maxgroups) and also prune entries that are timed out. */
	whowas_users::iterator iter;
	int fifosize;
	while ((fifosize = (int)whowas_fifo.size()) > 0)
	{
		if (fifosize > maxgroups || whowas_fifo[0].first < t - maxkeep)
		{
			iter = whowas.find(whowas_fifo[0].second);

			/* hopefully redundant integrity check, but added while debugging r6216 */
			if (iter == whowas.end())
			{
				/* this should never happen, if it does maps are corrupt */
				ServerInstance->Logs->Log("WHOWAS",LOG_DEFAULT, "BUG: Whowas maps got corrupted! (1)");
				return;
			}

			whowas_set* n = (whowas_set*)iter->second;

			if (n->size())
			{
				while (n->begin() != n->end())
				{
					WhoWasGroup *a = *(n->begin());
					delete a;
					n->pop_front();
				}
			}

			delete n;
			whowas.erase(iter);
			whowas_fifo.pop_front();
		}
		else
			break;
	}

	/* Then cut the whowas sets to new size (groupsize) */
	fifosize = (int)whowas_fifo.size();
	for (int i = 0; i < fifosize; i++)
	{
		iter = whowas.find(whowas_fifo[0].second);
		/* hopefully redundant integrity check, but added while debugging r6216 */
		if (iter == whowas.end())
		{
			/* this should never happen, if it does maps are corrupt */
			ServerInstance->Logs->Log("WHOWAS",LOG_DEFAULT, "BUG: Whowas maps got corrupted! (2)");
			return;
		}
		whowas_set* n = (whowas_set*)iter->second;
		if (n->size())
		{
			int nickcount = n->size();
			while (n->begin() != n->end() && nickcount > groupsize)
			{
				WhoWasGroup *a = *(n->begin());
				delete a;
				n->pop_front();
				nickcount--;
			}
		}
	}
}

/* call maintain once an hour to remove expired nicks */
void CommandWhowas::MaintainWhoWas(time_t t)
{
	for (whowas_users::iterator iter = whowas.begin(); iter != whowas.end(); iter++)
	{
		whowas_set* n = (whowas_set*)iter->second;
		if (n->size())
		{
			while ((n->begin() != n->end()) && ((*n->begin())->signon < t - this->WhoWasMaxKeep))
			{
				WhoWasGroup *a = *(n->begin());
				delete a;
				n->erase(n->begin());
			}
		}
	}
}

CommandWhowas::~CommandWhowas()
{
	whowas_users::iterator iter;
	int fifosize;
	while ((fifosize = (int)whowas_fifo.size()) > 0)
	{
		iter = whowas.find(whowas_fifo[0].second);

		/* hopefully redundant integrity check, but added while debugging r6216 */
		if (iter == whowas.end())
		{
			/* this should never happen, if it does maps are corrupt */
			ServerInstance->Logs->Log("WHOWAS",LOG_DEFAULT, "BUG: Whowas maps got corrupted! (3)");
			return;
		}

		whowas_set* n = (whowas_set*)iter->second;

		if (n->size())
		{
			while (n->begin() != n->end())
			{
				WhoWasGroup *a = *(n->begin());
				delete a;
				n->pop_front();
			}
		}

		delete n;
		whowas.erase(iter);
		whowas_fifo.pop_front();
	}
}

WhoWasGroup::WhoWasGroup(User* user) : host(user->host), dhost(user->dhost), ident(user->ident),
	server(user->server), gecos(user->fullname), signon(user->signon)
{
}

WhoWasGroup::~WhoWasGroup()
{
}

class ModuleWhoWas : public Module
{
	CommandWhowas cmd;

	void RangeCheck(int& value, int min, int max, int def, const char* msg)
	{
		// From ConfigReader
		if (value >= min && value <= max)
			return;

		ServerInstance->Logs->Log("CONFIG", LOG_DEFAULT, "WARNING: %s value of %d is not between %d and %d; set to %d.", msg, value, min, max, def);
		value = def;
	}

 public:
	ModuleWhoWas() : cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		Implementation eventlist[] = { I_OnGarbageCollect, I_OnUserQuit, I_OnStats, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	void OnGarbageCollect()
	{
		/* Removes all entries older than WhoWasMaxKeep */
		cmd.MaintainWhoWas(ServerInstance->Time());
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

	void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("whowas");
		int NewGroupSize = tag->getInt("groupsize");
		int NewMaxGroups = tag->getInt("maxgroups");
		int NewMaxKeep = InspIRCd::Duration(tag->getString("maxkeep"));

		RangeCheck(NewGroupSize, 0, 10000, 10, "<whowas:groupsize>");
		RangeCheck(NewMaxGroups, 0, 1000000, 10240, "<whowas:maxgroups>");
		RangeCheck(NewMaxKeep, 3600, INT_MAX, 3600, "<whowas:maxkeep>");

		if ((NewGroupSize == cmd.WhoWasGroupSize) && (NewMaxGroups == cmd.WhoWasMaxGroups) && (NewMaxKeep == cmd.WhoWasMaxKeep))
			return;

		cmd.WhoWasGroupSize = NewGroupSize;
		cmd.WhoWasMaxGroups = NewMaxGroups;
		cmd.WhoWasMaxKeep = NewMaxKeep;
		cmd.PruneWhoWas(ServerInstance->Time());
	}

	Version GetVersion()
	{
		return Version("WHOWAS", VF_VENDOR);
	}
};

MODULE_INIT(ModuleWhoWas)
