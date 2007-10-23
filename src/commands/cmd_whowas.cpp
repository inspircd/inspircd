/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_whowas.h"

WhoWasMaintainTimer * timer;

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandWhowas(Instance);
}

CommandWhowas::CommandWhowas(InspIRCd* Instance) : Command(Instance, "WHOWAS", 0, 1, false, 2)
{
	syntax = "<nick>{,<nick>}";
	timer = new WhoWasMaintainTimer(Instance, 3600);
	Instance->Timers->AddTimer(timer);
}

CmdResult CommandWhowas::Handle (const char** parameters, int, User* user)
{
	/* if whowas disabled in config */
	if (ServerInstance->Config->WhoWasGroupSize == 0 || ServerInstance->Config->WhoWasMaxGroups == 0)
	{
		user->WriteServ("421 %s %s :This command has been disabled.",user->nick,command.c_str());
		return CMD_FAILURE;
	}

	whowas_users::iterator i = whowas.find(parameters[0]);

	if (i == whowas.end())
	{
		user->WriteServ("406 %s %s :There was no such nickname",user->nick,parameters[0]);
		user->WriteServ("369 %s %s :End of WHOWAS",user->nick,parameters[0]);
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
				char b[MAXBUF];
	
				timeinfo = localtime(&rawtime);
				
				/* XXX - 'b' could be only 25 chars long and then strlcpy() would terminate it for us too? */
				strlcpy(b,asctime(timeinfo),MAXBUF);
				b[24] = 0;

				user->WriteServ("314 %s %s %s %s * :%s",user->nick,parameters[0],u->ident,u->dhost,u->gecos);
				
				if (IS_OPER(user))
					user->WriteServ("379 %s %s :was connecting from *@%s", user->nick, parameters[0], u->host);
				
				if (*ServerInstance->Config->HideWhoisServer && !IS_OPER(user))
					user->WriteServ("312 %s %s %s :%s",user->nick,parameters[0], ServerInstance->Config->HideWhoisServer, b);
				else
					user->WriteServ("312 %s %s %s :%s",user->nick,parameters[0], u->server, b);
			}
		}
		else
		{
			user->WriteServ("406 %s %s :There was no such nickname",user->nick,parameters[0]);
			user->WriteServ("369 %s %s :End of WHOWAS",user->nick,parameters[0]);
			return CMD_FAILURE;
		}
	}

	user->WriteServ("369 %s %s :End of WHOWAS",user->nick,parameters[0]);
	return CMD_SUCCESS;
}

CmdResult CommandWhowas::HandleInternal(const unsigned int id, const std::deque<classbase*> &parameters)
{
	switch (id)
	{
		case WHOWAS_ADD:
			AddToWhoWas((User*)parameters[0]);
		break;

		case WHOWAS_STATS:
			GetStats((Extensible*)parameters[0]);
		break;

		case WHOWAS_PRUNE:
			PruneWhoWas(ServerInstance->Time());
		break;

		case WHOWAS_MAINTAIN:
			MaintainWhoWas(ServerInstance->Time());
		break;

		default:
		break;
	}
	return CMD_SUCCESS;
}

void CommandWhowas::GetStats(Extensible* ext)
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
	ext->Extend("stats", stats.c_str());
}

void CommandWhowas::AddToWhoWas(User* user)
{
	/* if whowas disabled */
	if (ServerInstance->Config->WhoWasGroupSize == 0 || ServerInstance->Config->WhoWasMaxGroups == 0)
	{
		return;
	}

	whowas_users::iterator iter = whowas.find(user->nick);

	if (iter == whowas.end())
	{
		whowas_set* n = new whowas_set;
		WhoWasGroup *a = new WhoWasGroup(user);
		n->push_back(a);
		whowas[user->nick] = n;
		whowas_fifo.push_back(std::make_pair(ServerInstance->Time(),user->nick));

		if ((int)(whowas.size()) > ServerInstance->Config->WhoWasMaxGroups)
		{
			whowas_users::iterator iter = whowas.find(whowas_fifo[0].second);
			if (iter != whowas.end())
			{
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
			WhoWasGroup *a = (WhoWasGroup*)*(group->begin());
			delete a;
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
				ServerInstance->Log(DEFAULT, "BUG: Whowas maps got corrupted! (1)");
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
			ServerInstance->Log(DEFAULT, "BUG: Whowas maps got corrupted! (2)");
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
			ServerInstance->Log(DEFAULT, "BUG: Whowas maps got corrupted! (3)");
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

WhoWasGroup::WhoWasGroup(User* user) : host(NULL), dhost(NULL), ident(NULL), server(NULL), gecos(NULL), signon(user->signon)
{
	this->host = strdup(user->host);
	this->dhost = strdup(user->dhost);
	this->ident = strdup(user->ident);
	this->server = user->server;
	this->gecos = strdup(user->fullname);
}

WhoWasGroup::~WhoWasGroup()
{
	if (host)
		free(host);
	if (dhost)
		free(dhost);
	if (ident)
		free(ident);
	if (gecos)
		free(gecos);
}

/* every hour, run this function which removes all entries older than Config->WhoWasMaxKeep */
void WhoWasMaintainTimer::Tick(time_t)
{
	Command* whowas_command = ServerInstance->Parser->GetHandler("WHOWAS");
	if (whowas_command)
	{
		std::deque<classbase*> params;
		whowas_command->HandleInternal(WHOWAS_MAINTAIN, params);
	}
}
