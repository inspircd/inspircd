/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "timer.h"

/* Forward ref for timer */
class WhoWasMaintainTimer;

/* Forward ref for typedefs */
class WhoWasGroup;

/** A group of users related by nickname
 */
typedef std::deque<WhoWasGroup*> whowas_set;

/** Sets of users in the whowas system
 */
typedef std::map<irc::string,whowas_set*> whowas_users;

/** Sets of time and users in whowas list
 */
typedef std::deque<std::pair<time_t,irc::string> > whowas_users_fifo;

class WhoWasMaintainerImpl : public WhoWasMaintainer
{
 public:
	/** Whowas container, contains a map of vectors of users tracked by WHOWAS
	 */
	whowas_users whowas;

	/** Whowas container, contains a map of time_t to users tracked by WHOWAS
	 */
	whowas_users_fifo whowas_fifo;

	WhoWasMaintainerImpl(Module* mod) : WhoWasMaintainer(mod) {}
	void AddToWhoWas(User* user);
	std::string GetStats();
	void PruneWhoWas(time_t t);
	void MaintainWhoWas(time_t t);
};

/** Handle /WHOWAS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandWhowas : public Command
{
  public:
	WhoWasMaintainerImpl prov;
	CommandWhowas(Module* parent);
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
	~CommandWhowas();
};

/** Used to hold WHOWAS information
 */
class WhoWasGroup
{
 public:
	/** Real host
	 */
	std::string host;
	/** Displayed host
	 */
	std::string dhost;
	/** Ident
	 */
	std::string ident;
	/** Server name
	 */
	std::string server;
	/** Fullname (GECOS)
	 */
	std::string gecos;
	/** Signon time
	 */
	time_t signon;

	/** Initialize this WhoWasFroup with a user
	 */
	WhoWasGroup(User* user);
	/** Destructor
	 */
	~WhoWasGroup();
};

class WhoWasMaintainTimer : public Timer
{
  public:
	CommandWhowas cmd;
	WhoWasMaintainTimer(Module* m) : Timer(3600, ServerInstance->Time(), true), cmd(m)
	{
	}
	virtual void Tick(time_t TIME);
};



CommandWhowas::CommandWhowas( Module* parent) : Command(parent, "WHOWAS", 1), prov(parent)
{
	syntax = "<nick>{,<nick>}";
	Penalty = 2;
}

CmdResult CommandWhowas::Handle (const std::vector<std::string>& parameters, User* user)
{
	/* if whowas disabled in config */
	if (ServerInstance->Config->WhoWasGroupSize == 0 || ServerInstance->Config->WhoWasMaxGroups == 0)
	{
		user->WriteNumeric(421, "%s %s :This command has been disabled.",user->nick.c_str(),name.c_str());
		return CMD_FAILURE;
	}

	whowas_users::iterator i = prov.whowas.find(parameters[0]);

	if (i == prov.whowas.end())
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

std::string WhoWasMaintainerImpl::GetStats()
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

void WhoWasMaintainerImpl::AddToWhoWas(User* user)
{
	/* if whowas disabled */
	if (ServerInstance->Config->WhoWasGroupSize == 0 || ServerInstance->Config->WhoWasMaxGroups == 0)
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

		if ((int)(whowas.size()) > ServerInstance->Config->WhoWasMaxGroups)
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

		if ((int)(group->size()) > ServerInstance->Config->WhoWasGroupSize)
		{
			WhoWasGroup *a2 = (WhoWasGroup*)*(group->begin());
			delete a2;
			group->pop_front();
		}
	}
}

/* on rehash, refactor maps according to new conf values */
void WhoWasMaintainerImpl::PruneWhoWas(time_t t)
{
	/* config values */
	int groupsize = ServerInstance->Config->WhoWasGroupSize;
	int maxgroups = ServerInstance->Config->WhoWasMaxGroups;
	int maxkeep =   ServerInstance->Config->WhoWasMaxKeep;

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
				ServerInstance->Logs->Log("WHOWAS",DEFAULT, "BUG: Whowas maps got corrupted! (1)");
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
			ServerInstance->Logs->Log("WHOWAS",DEFAULT, "BUG: Whowas maps got corrupted! (2)");
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
void WhoWasMaintainerImpl::MaintainWhoWas(time_t t)
{
	for (whowas_users::iterator iter = whowas.begin(); iter != whowas.end(); iter++)
	{
		whowas_set* n = (whowas_set*)iter->second;
		if (n->size())
		{
			while ((n->begin() != n->end()) && ((*n->begin())->signon < t - ServerInstance->Config->WhoWasMaxKeep))
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
	while ((fifosize = (int)prov.whowas_fifo.size()) > 0)
	{
		iter = prov.whowas.find(prov.whowas_fifo[0].second);

		/* hopefully redundant integrity check, but added while debugging r6216 */
		if (iter == prov.whowas.end())
		{
			/* this should never happen, if it does maps are corrupt */
			ServerInstance->Logs->Log("WHOWAS",DEFAULT, "BUG: Whowas maps got corrupted! (3)");
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
		prov.whowas.erase(iter);
		prov.whowas_fifo.pop_front();
	}
}

WhoWasGroup::WhoWasGroup(User* user) : host(user->host), dhost(user->dhost), ident(user->ident),
	server(user->server), gecos(user->fullname), signon(user->signon)
{
}

WhoWasGroup::~WhoWasGroup()
{
}

/* every hour, run this function which removes all entries older than Config->WhoWasMaxKeep */
void WhoWasMaintainTimer::Tick(time_t now)
{
	cmd.prov.MaintainWhoWas(now);
}

class ModuleWhoWas : public Module
{
	WhoWasMaintainTimer* timer;
 public:
	ModuleWhoWas() : timer(new WhoWasMaintainTimer(this)) {}

	void init()
	{
		ServerInstance->Timers->AddTimer(timer);
		ServerInstance->Modules->AddService(timer->cmd);
		ServerInstance->Modules->AddService(timer->cmd.prov);
	}

	~ModuleWhoWas()
	{
		ServerInstance->Timers->DelTimer(timer);
	}

	Version GetVersion()
	{
		return Version("WHOWAS Command", VF_VENDOR);
	}
};

MODULE_INIT(ModuleWhoWas)
