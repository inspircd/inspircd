/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_whowas.h"

WhoWasMaintainTimer * timer;

CommandWhowas::CommandWhowas( Module* parent) : Command(parent, "WHOWAS", 1)
{
	syntax = "<nick>{,<nick>}";
	Penalty = 2;
	timer = new WhoWasMaintainTimer(ServerInstance, 3600);
	ServerInstance->Timers->AddTimer(timer);
}

CmdResult CommandWhowas::Handle (const std::vector<std::string>& parameters, User* user)
{
	/* if whowas disabled in config */
	if (ServerInstance->Config->WhoWasGroupSize == 0 || ServerInstance->Config->WhoWasMaxGroups == 0)
	{
		user->WriteNumeric(421, "%s %s :This command has been disabled.",user->nick.c_str(),command.c_str());
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

				if (*ServerInstance->Config->HideWhoisServer && !user->HasPrivPermission("servers/auspex"))
					user->WriteNumeric(312, "%s %s %s :%s",user->nick.c_str(),parameters[0].c_str(), ServerInstance->Config->HideWhoisServer, b);
				else
					user->WriteNumeric(312, "%s %s %s :%s",user->nick.c_str(),parameters[0].c_str(), u->server, b);
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
	stats.assign("Whowas(MAPSETS) " +ConvToStr(whowas_size)+" ("+ConvToStr(whowas_bytes)+" bytes)");
	return stats;
}

void CommandWhowas::AddToWhoWas(User* user)
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
void CommandWhowas::PruneWhoWas(time_t t)
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
void CommandWhowas::MaintainWhoWas(time_t t)
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
	if (timer)
	{
		ServerInstance->Timers->DelTimer(timer);
	}

	whowas_users::iterator iter;
	int fifosize;
	while ((fifosize = (int)whowas_fifo.size()) > 0)
	{
		iter = whowas.find(whowas_fifo[0].second);

		/* hopefully redundant integrity check, but added while debugging r6216 */
		if (iter == whowas.end())
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

/* every hour, run this function which removes all entries older than Config->WhoWasMaxKeep */
void WhoWasMaintainTimer::Tick(time_t)
{
	Module* whowas = ServerInstance->Modules->Find("cmd_whowas.so");
	if (whowas)
	{
		WhowasRequest(whowas, whowas, WhowasRequest::WHOWAS_MAINTAIN).Send();
	}
}

class ModuleWhoWas : public Module
{
	CommandWhowas cmd;
 public:
	ModuleWhoWas(InspIRCd *Me) : Module(Me), cmd(this)
	{
		ServerInstance->AddCommand(&cmd);
	}

	const char* OnRequest(Request* request)
	{
		WhowasRequest* req = static_cast<WhowasRequest*>(request);
		switch (req->type)
		{
			case WhowasRequest::WHOWAS_ADD:
				cmd.AddToWhoWas(req->user);
				break;
			case WhowasRequest::WHOWAS_STATS:
				req->value = cmd.GetStats();
				break;
			case WhowasRequest::WHOWAS_PRUNE:
				cmd.PruneWhoWas(ServerInstance->Time());
				break;
			case WhowasRequest::WHOWAS_MAINTAIN:
				cmd.MaintainWhoWas(ServerInstance->Time());
				break;
		}
		return NULL;
	}

	Version GetVersion()
	{
		return Version("WHOWAS Command", VF_VENDOR);
	}
};

MODULE_INIT(ModuleWhoWas)
