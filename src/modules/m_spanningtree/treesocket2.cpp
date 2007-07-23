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
#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "wildcard.h"
#include "xline.h"
#include "transport.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/handshaketimer.h"

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

static std::map<std::string, std::string> warned;       /* Server names that have had protocol violation warnings displayed for them */

int TreeSocket::WriteLine(std::string line)
{
	Instance->Log(DEBUG, "S[%d] -> %s", this->GetFd(), line.c_str());
	line.append("\r\n");
	return this->Write(line);
}


/* Handle ERROR command */
bool TreeSocket::Error(std::deque<std::string> &params)
{
	if (params.size() < 1)
		return false;
	this->Instance->SNO->WriteToSnoMask('l',"ERROR from %s: %s",(!InboundServerName.empty() ? InboundServerName.c_str() : myhost.c_str()),params[0].c_str());
	/* we will return false to cause the socket to close. */
	return false;
}

bool TreeSocket::Modules(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.empty())
		return true;

	if (!this->Instance->MatchText(this->Instance->Config->ServerName, params[0]))
	{
		/* Pass it on, not for us */
		Utils->DoOneToOne(prefix, "MODULES", params, params[0]);
		return true;
	}

	char strbuf[MAXBUF];
	std::deque<std::string> par;
	par.push_back(prefix);
	par.push_back("");

	userrec* source = this->Instance->FindNick(prefix);
	if (!source)
		return true;

	for (unsigned int i = 0; i < Instance->Config->module_names.size(); i++)
	{
		Version V = Instance->modules[i]->GetVersion();
		char modulename[MAXBUF];
		char flagstate[MAXBUF];
		*flagstate = 0;
		if (V.Flags & VF_STATIC)
			strlcat(flagstate,", static",MAXBUF);
		if (V.Flags & VF_VENDOR)
			strlcat(flagstate,", vendor",MAXBUF);
		if (V.Flags & VF_COMMON)
			strlcat(flagstate,", common",MAXBUF);
		if (V.Flags & VF_SERVICEPROVIDER)
			strlcat(flagstate,", service provider",MAXBUF);
		if (!flagstate[0])
			strcpy(flagstate,"  <no flags>");
		strlcpy(modulename,Instance->Config->module_names[i].c_str(),256);
		if (*source->oper)
		{
			snprintf(strbuf, MAXBUF, "::%s 900 %s :0x%08lx %d.%d.%d.%d %s (%s)",Instance->Config->ServerName,source->nick,(long unsigned int)Instance->modules[i],V.Major,V.Minor,V.Revision,V.Build,ServerConfig::CleanFilename(modulename),flagstate+2);
		}
		else
		{
			snprintf(strbuf, MAXBUF, "::%s 900 %s :%s",Instance->Config->ServerName,source->nick,ServerConfig::CleanFilename(modulename));
		}
		par[1] = strbuf;
		Utils->DoOneToOne(Instance->Config->ServerName, "PUSH", par, source->server);
	}
	snprintf(strbuf, MAXBUF, "::%s 901 %s :End of MODULES list", Instance->Config->ServerName, source->nick);
	par[1] = strbuf;
	Utils->DoOneToOne(Instance->Config->ServerName, "PUSH", par, source->server);
	return true;
}

/** remote MOTD. leet, huh? */
bool TreeSocket::Motd(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() > 0)
	{
		if (this->Instance->MatchText(this->Instance->Config->ServerName, params[0]))
		{
			/* It's for our server */
			string_list results;
			userrec* source = this->Instance->FindNick(prefix);

			if (source)
			{
				std::deque<std::string> par;
				par.push_back(prefix);
				par.push_back("");

				if (!Instance->Config->MOTD.size())
				{
					par[1] = std::string("::")+Instance->Config->ServerName+" 422 "+source->nick+" :Message of the day file is missing.";
					Utils->DoOneToOne(this->Instance->Config->ServerName, "PUSH",par, source->server);
					return true;
				}

				par[1] = std::string("::")+Instance->Config->ServerName+" 375 "+source->nick+" :"+Instance->Config->ServerName+" message of the day";
				Utils->DoOneToOne(this->Instance->Config->ServerName, "PUSH",par, source->server);

				for (unsigned int i = 0; i < Instance->Config->MOTD.size(); i++)
				{
					par[1] = std::string("::")+Instance->Config->ServerName+" 372 "+source->nick+" :- "+Instance->Config->MOTD[i];
					Utils->DoOneToOne(this->Instance->Config->ServerName, "PUSH",par, source->server);
				}

				par[1] = std::string("::")+Instance->Config->ServerName+" 376 "+source->nick+" End of message of the day.";
				Utils->DoOneToOne(this->Instance->Config->ServerName, "PUSH",par, source->server);
			}
		}
		else
		{
			/* Pass it on */
			userrec* source = this->Instance->FindNick(prefix);
			if (source)
				Utils->DoOneToOne(prefix, "MOTD", params, params[0]);
		}
	}
	return true;
}

/** remote ADMIN. leet, huh? */
bool TreeSocket::Admin(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() > 0)
	{
		if (this->Instance->MatchText(this->Instance->Config->ServerName, params[0]))
		{
			/* It's for our server */
			string_list results;
			userrec* source = this->Instance->FindNick(prefix);
			if (source)
			{
				std::deque<std::string> par;
				par.push_back(prefix);
				par.push_back("");
				par[1] = std::string("::")+Instance->Config->ServerName+" 256 "+source->nick+" :Administrative info for "+Instance->Config->ServerName;
				Utils->DoOneToOne(this->Instance->Config->ServerName, "PUSH",par, source->server);
				par[1] = std::string("::")+Instance->Config->ServerName+" 257 "+source->nick+" :Name     - "+Instance->Config->AdminName;
				Utils->DoOneToOne(this->Instance->Config->ServerName, "PUSH",par, source->server);
				par[1] = std::string("::")+Instance->Config->ServerName+" 258 "+source->nick+" :Nickname - "+Instance->Config->AdminNick;
				Utils->DoOneToOne(this->Instance->Config->ServerName, "PUSH",par, source->server);
				par[1] = std::string("::")+Instance->Config->ServerName+" 258 "+source->nick+" :E-Mail   - "+Instance->Config->AdminEmail;
				Utils->DoOneToOne(this->Instance->Config->ServerName, "PUSH",par, source->server);
			}
		}
		else
		{
			/* Pass it on */
			userrec* source = this->Instance->FindNick(prefix);
			if (source)
				Utils->DoOneToOne(prefix, "ADMIN", params, params[0]);
		}
	}
	return true;
}

bool TreeSocket::Stats(const std::string &prefix, std::deque<std::string> &params)
{
	/* Get the reply to a STATS query if it matches this servername,
	 * and send it back as a load of PUSH queries
	 */
	if (params.size() > 1)
	{
		if (this->Instance->MatchText(this->Instance->Config->ServerName, params[1]))
		{
			/* It's for our server */
			string_list results;
			userrec* source = this->Instance->FindNick(prefix);
			if (source)
			{
				std::deque<std::string> par;
				par.push_back(prefix);
				par.push_back("");
				DoStats(this->Instance, *(params[0].c_str()), source, results);
				for (size_t i = 0; i < results.size(); i++)
				{
					par[1] = "::" + results[i];
					Utils->DoOneToOne(this->Instance->Config->ServerName, "PUSH",par, source->server);
				}
			}
		}
		else
		{
			/* Pass it on */
			userrec* source = this->Instance->FindNick(prefix);
			if (source)
				Utils->DoOneToOne(prefix, "STATS", params, params[1]);
		}
	}
	return true;
}


/** Because the core won't let users or even SERVERS set +o,
 * we use the OPERTYPE command to do this.
 */
bool TreeSocket::OperType(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() != 1)
		return true;
	std::string opertype = params[0];
	userrec* u = this->Instance->FindNick(prefix);
	if (u)
	{
		u->modes[UM_OPERATOR] = 1;
		this->Instance->all_opers.push_back(u);
		strlcpy(u->oper,opertype.c_str(),NICKMAX-1);
		Utils->DoOneToAllButSender(u->nick,"OPERTYPE",params,u->server);
		this->Instance->SNO->WriteToSnoMask('o',"From %s: User %s (%s@%s) is now an IRC operator of type %s",u->server, u->nick,u->ident,u->host,irc::Spacify(opertype.c_str()));
	}
	return true;
}

/** Because Andy insists that services-compatible servers must
 * implement SVSNICK and SVSJOIN, that's exactly what we do :p
 */
bool TreeSocket::ForceNick(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 3)
		return true;

	userrec* u = this->Instance->FindNick(params[0]);

	if (u)
	{
		Utils->DoOneToAllButSender(prefix,"SVSNICK",params,prefix);
		if (IS_LOCAL(u))
		{
			std::deque<std::string> par;
			par.push_back(params[1]);
			if (!u->ForceNickChange(params[1].c_str()))
			{
				userrec::QuitUser(this->Instance, u, "Nickname collision");
				return true;
			}
			u->age = atoi(params[2].c_str());
		}
	}
	return true;
}

bool TreeSocket::OperQuit(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return true;

	userrec* u = this->Instance->FindNick(prefix);

	if (u)
	{
		u->SetOperQuit(params[0]);
		params[0] = ":" + params[0];
		Utils->DoOneToAllButSender(prefix,"OPERQUIT",params,prefix);
	}
	return true;
}

bool TreeSocket::ServiceJoin(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 2)
		return true;

	userrec* u = this->Instance->FindNick(params[0]);

	if (u)
	{
		/* only join if it's local, otherwise just pass it on! */
		if (IS_LOCAL(u))
			chanrec::JoinUser(this->Instance, u, params[1].c_str(), false, "", Instance->Time());
		Utils->DoOneToAllButSender(prefix,"SVSJOIN",params,prefix);
	}
	return true;
}

bool TreeSocket::RemoteRehash(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return false;

	std::string servermask = params[0];

	if (this->Instance->MatchText(this->Instance->Config->ServerName,servermask))
	{
		this->Instance->SNO->WriteToSnoMask('l',"Remote rehash initiated by \002"+prefix+"\002.");
		this->Instance->RehashServer();
		Utils->ReadConfiguration(false);
		InitializeDisabledCommands(Instance->Config->DisabledCommands, Instance);
	}
	Utils->DoOneToAllButSender(prefix,"REHASH",params,prefix);
	return true;
}

bool TreeSocket::RemoteKill(const std::string &prefix, std::deque<std::string> &params)
{ 	 
	if (params.size() != 2)
		return true;

	userrec* who = this->Instance->FindNick(params[0]);

	if (who)
	{
		/* Prepend kill source, if we don't have one */ 	 
		if (*(params[1].c_str()) != '[')
		{
			params[1] = "[" + prefix + "] Killed (" + params[1] +")";
		}
		std::string reason = params[1];
		params[1] = ":" + params[1];
		Utils->DoOneToAllButSender(prefix,"KILL",params,prefix);
		// NOTE: This is safe with kill hiding on, as RemoteKill is only reached if we have a server prefix.
		// in short this is not executed for USERS.
		who->Write(":%s KILL %s :%s (%s)", prefix.c_str(), who->nick, prefix.c_str(), reason.c_str());
		userrec::QuitUser(this->Instance,who,reason);
	}
	return true;
}

bool TreeSocket::LocalPong(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return true;

	if (params.size() == 1)
	{
		TreeServer* ServerSource = Utils->FindServer(prefix);
		if (ServerSource)
		{
			ServerSource->SetPingFlag();
			ServerSource->rtt = Instance->Time() - ServerSource->LastPing;
		}
	}
	else
	{
		std::string forwardto = params[1];
		if (forwardto == this->Instance->Config->ServerName)
		{
			/*
			 * this is a PONG for us
			 * if the prefix is a user, check theyre local, and if they are,
			 * dump the PONG reply back to their fd. If its a server, do nowt.
			 * Services might want to send these s->s, but we dont need to yet.
			 */
			userrec* u = this->Instance->FindNick(prefix);
			if (u)
			{
				u->WriteServ("PONG %s %s",params[0].c_str(),params[1].c_str());
			}
		}
		else
		{
			// not for us, pass it on :)
			Utils->DoOneToOne(prefix,"PONG",params,forwardto);
		}
	}

	return true;
}

bool TreeSocket::MetaData(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 2)
		return true;
	else if (params.size() < 3)
		params.push_back("");
	TreeServer* ServerSource = Utils->FindServer(prefix);
	if (ServerSource)
	{
		Utils->SetRemoteBursting(ServerSource, false);

		if (params[0] == "*")
		{
			FOREACH_MOD_I(this->Instance,I_OnDecodeMetaData,OnDecodeMetaData(TYPE_OTHER,NULL,params[1],params[2]));
		}
		else if (*(params[0].c_str()) == '#')
		{
			chanrec* c = this->Instance->FindChan(params[0]);
			if (c)
			{
				FOREACH_MOD_I(this->Instance,I_OnDecodeMetaData,OnDecodeMetaData(TYPE_CHANNEL,c,params[1],params[2]));
			}
		}
		else if (*(params[0].c_str()) != '#')
		{
			userrec* u = this->Instance->FindNick(params[0]);
			if (u)
			{
				FOREACH_MOD_I(this->Instance,I_OnDecodeMetaData,OnDecodeMetaData(TYPE_USER,u,params[1],params[2]));
			}
		}
	}

	params[2] = ":" + params[2];
	Utils->DoOneToAllButSender(prefix,"METADATA",params,prefix);
	return true;
}

bool TreeSocket::ServerVersion(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return true;

	TreeServer* ServerSource = Utils->FindServer(prefix);

	if (ServerSource)
	{
		ServerSource->SetVersion(params[0]);
	}
	params[0] = ":" + params[0];
	Utils->DoOneToAllButSender(prefix,"VERSION",params,prefix);
	return true;
}

bool TreeSocket::ChangeHost(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return true;
	userrec* u = this->Instance->FindNick(prefix);

	if (u)
	{
		u->ChangeDisplayedHost(params[0].c_str());
		Utils->DoOneToAllButSender(prefix,"FHOST",params,u->server);
	}
	return true;
}

bool TreeSocket::AddLine(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 6)
		return true;
	bool propogate = false;
	if (!this->bursting)
		Utils->lines_to_apply = 0;
	switch (*(params[0].c_str()))
	{
		case 'Z':
			propogate = Instance->XLines->add_zline(atoi(params[4].c_str()), params[2].c_str(), params[5].c_str(), params[1].c_str());
			Instance->XLines->zline_set_creation_time(params[1].c_str(), atoi(params[3].c_str()));
			if (propogate)
				Utils->lines_to_apply |= APPLY_ZLINES;
		break;
		case 'Q':
			propogate = Instance->XLines->add_qline(atoi(params[4].c_str()), params[2].c_str(), params[5].c_str(), params[1].c_str());
			Instance->XLines->qline_set_creation_time(params[1].c_str(), atoi(params[3].c_str()));
			if (propogate)
				Utils->lines_to_apply |= APPLY_QLINES;
		break;
		case 'E':
			propogate = Instance->XLines->add_eline(atoi(params[4].c_str()), params[2].c_str(), params[5].c_str(), params[1].c_str());
			Instance->XLines->eline_set_creation_time(params[1].c_str(), atoi(params[3].c_str()));
		break;
		case 'G':
			propogate = Instance->XLines->add_gline(atoi(params[4].c_str()), params[2].c_str(), params[5].c_str(), params[1].c_str());
			Instance->XLines->gline_set_creation_time(params[1].c_str(), atoi(params[3].c_str()));
			if (propogate)
				Utils->lines_to_apply |= APPLY_GLINES;
		break;
		case 'K':
			propogate = Instance->XLines->add_kline(atoi(params[4].c_str()), params[2].c_str(), params[5].c_str(), params[1].c_str());
			if (propogate)
				Utils->lines_to_apply |= APPLY_KLINES;
		break;
		default:
			/* Just in case... */
			this->Instance->SNO->WriteToSnoMask('x',"\2WARNING\2: Invalid xline type '"+params[0]+"' sent by server "+prefix+", ignored!");
			propogate = false;
		break;
	}
	/* Send it on its way */
	if (propogate)
	{
		if (atoi(params[4].c_str()))
		{
			time_t c_requires_crap = ConvToInt(params[4]) + Instance->Time();
			this->Instance->SNO->WriteToSnoMask('x',"%s Added %cLINE on %s to expire on %s (%s).",prefix.c_str(),*(params[0].c_str()),params[1].c_str(),Instance->TimeString(c_requires_crap).c_str(),params[5].c_str());
		}
		else
		{
			this->Instance->SNO->WriteToSnoMask('x',"%s Added permenant %cLINE on %s (%s).",prefix.c_str(),*(params[0].c_str()),params[1].c_str(),params[5].c_str());
		}
		params[5] = ":" + params[5];
		Utils->DoOneToAllButSender(prefix,"ADDLINE",params,prefix);
	}
	if (!this->bursting)
	{
		Instance->XLines->apply_lines(Utils->lines_to_apply);
		Utils->lines_to_apply = 0;
	}
	return true;
}

bool TreeSocket::ChangeName(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return true;
	userrec* u = this->Instance->FindNick(prefix);
	if (u)
	{
		u->ChangeName(params[0].c_str());
		params[0] = ":" + params[0];
		Utils->DoOneToAllButSender(prefix,"FNAME",params,u->server);
	}
	return true;
}

bool TreeSocket::Whois(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return true;
	userrec* u = this->Instance->FindNick(prefix);
	if (u)
	{
		// an incoming request
		if (params.size() == 1)
		{
			userrec* x = this->Instance->FindNick(params[0]);
			if ((x) && (IS_LOCAL(x)))
			{
				userrec* x = this->Instance->FindNick(params[0]);
				char signon[MAXBUF];
				char idle[MAXBUF];
				snprintf(signon, MAXBUF, "%lu", (unsigned long)x->signon);
				snprintf(idle, MAXBUF, "%lu", (unsigned long)abs((x->idle_lastmsg) - Instance->Time(true)));
				std::deque<std::string> par;
				par.push_back(prefix);
				par.push_back(signon);
				par.push_back(idle);
				// ours, we're done, pass it BACK
				Utils->DoOneToOne(params[0], "IDLE", par, u->server);
			}
			else
			{
				// not ours pass it on
				if (x)
					Utils->DoOneToOne(prefix, "IDLE", params, x->server);
			}
		}
		else if (params.size() == 3)
		{
			std::string who_did_the_whois = params[0];
			userrec* who_to_send_to = this->Instance->FindNick(who_did_the_whois);
			if ((who_to_send_to) && (IS_LOCAL(who_to_send_to)))
			{
				// an incoming reply to a whois we sent out
				std::string nick_whoised = prefix;
				unsigned long signon = atoi(params[1].c_str());
				unsigned long idle = atoi(params[2].c_str());
				if ((who_to_send_to) && (IS_LOCAL(who_to_send_to)))
				{
					do_whois(this->Instance, who_to_send_to, u, signon, idle, nick_whoised.c_str());
				}
			}
			else
			{
				// not ours, pass it on
				if (who_to_send_to)
					Utils->DoOneToOne(prefix, "IDLE", params, who_to_send_to->server);
			}
		}
	}
	return true;
}

bool TreeSocket::Push(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 2)
		return true;
	userrec* u = this->Instance->FindNick(params[0]);
	if (!u)
		return true;
	if (IS_LOCAL(u))
	{
		u->Write(params[1]);
	}
	else
	{
		// continue the raw onwards
		params[1] = ":" + params[1];
		Utils->DoOneToOne(prefix,"PUSH",params,u->server);
	}
	return true;
}

bool TreeSocket::HandleSetTime(const std::string &prefix, std::deque<std::string> &params)
{
	if (!params.size() || !Utils->EnableTimeSync)
		return true;

	bool force = false;

	if ((params.size() == 2) && (params[1] == "FORCE"))
		force = true;

	time_t them = atoi(params[0].c_str());
	time_t us = Instance->Time(false);

	time_t diff = them - us;

	Utils->DoOneToAllButSender(prefix, "TIMESET", params, prefix);

	if (force || (them != us))
	{
		time_t old = Instance->SetTimeDelta(diff);
		Instance->Log(DEBUG, "TS (diff %d) from %s applied (old delta was %d)", diff, prefix.c_str(), old);
	}

	return true;
}

bool TreeSocket::Time(const std::string &prefix, std::deque<std::string> &params)
{
	// :source.server TIME remote.server sendernick
	// :remote.server TIME source.server sendernick TS
	if (params.size() == 2)
	{
		// someone querying our time?
		if (this->Instance->Config->ServerName == params[0])
		{
			userrec* u = this->Instance->FindNick(params[1]);
			if (u)
			{
				params.push_back(ConvToStr(Instance->Time(false)));
				params[0] = prefix;
				Utils->DoOneToOne(this->Instance->Config->ServerName,"TIME",params,params[0]);
			}
		}
		else
		{
			// not us, pass it on
			userrec* u = this->Instance->FindNick(params[1]);
			if (u)
				Utils->DoOneToOne(prefix,"TIME",params,params[0]);
		}
	}
	else if (params.size() == 3)
	{
		// a response to a previous TIME
		userrec* u = this->Instance->FindNick(params[1]);
		if ((u) && (IS_LOCAL(u)))
		{
			time_t rawtime = atol(params[2].c_str());
			struct tm * timeinfo;
			timeinfo = localtime(&rawtime);
			char tms[26];
			snprintf(tms,26,"%s",asctime(timeinfo));
			tms[24] = 0;
			u->WriteServ("391 %s %s :%s",u->nick,prefix.c_str(),tms);
		}
		else
		{
			if (u)
				Utils->DoOneToOne(prefix,"TIME",params,u->server);
		}
	}
	return true;
}

bool TreeSocket::LocalPing(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return true;
	if (params.size() == 1)
	{
		std::string stufftobounce = params[0];
		this->WriteLine(std::string(":")+this->Instance->Config->ServerName+" PONG "+stufftobounce);
		return true;
	}
	else
	{
		std::string forwardto = params[1];
		if (forwardto == this->Instance->Config->ServerName)
		{
			// this is a ping for us, send back PONG to the requesting server
			params[1] = params[0];
			params[0] = forwardto;
			Utils->DoOneToOne(forwardto,"PONG",params,params[1]);
		}
		else
		{
			// not for us, pass it on :)
			Utils->DoOneToOne(prefix,"PING",params,forwardto);
		}
		return true;
	}
}

/** TODO: This creates a total mess of output and needs to really use irc::modestacker.
 */
bool TreeSocket::RemoveStatus(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return true;
	chanrec* c = Instance->FindChan(params[0]);
	if (c)
	{
		for (char modeletter = 'A'; modeletter <= 'z'; modeletter++)
		{
			ModeHandler* mh = Instance->Modes->FindMode(modeletter, MODETYPE_CHANNEL);
			if (mh)
				mh->RemoveMode(c);
		}
	}
	return true;
}

bool TreeSocket::RemoteServer(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 4)
		return false;
	std::string servername = params[0];
	std::string password = params[1];
	// hopcount is not used for a remote server, we calculate this ourselves
	std::string description = params[3];
	TreeServer* ParentOfThis = Utils->FindServer(prefix);
	if (!ParentOfThis)
	{
		this->SendError("Protocol error - Introduced remote server from unknown server "+prefix);
		return false;
	}
	TreeServer* CheckDupe = Utils->FindServer(servername);
	if (CheckDupe)
	{
		this->SendError("Server "+servername+" already exists!");
		this->Instance->SNO->WriteToSnoMask('l',"Server \2"+servername+"\2 being introduced from \2" + prefix + "\2 denied, already exists. Closing link with " + prefix);
		return false;
	}
	Link* lnk = Utils->FindLink(servername);
	TreeServer* Node = new TreeServer(this->Utils,this->Instance,servername,description,ParentOfThis,NULL, lnk ? lnk->Hidden : false);
	ParentOfThis->AddChild(Node);
	params[3] = ":" + params[3];
	Utils->SetRemoteBursting(Node, true);
	Utils->DoOneToAllButSender(prefix,"SERVER",params,prefix);
	this->Instance->SNO->WriteToSnoMask('l',"Server \002"+prefix+"\002 introduced server \002"+servername+"\002 ("+description+")");
	return true;
}

bool TreeSocket::ComparePass(const std::string &ours, const std::string &theirs)
{
	if ((!strncmp(ours.c_str(), "HMAC-SHA256:", 12)) || (!strncmp(theirs.c_str(), "HMAC-SHA256:", 12)))
	{
		/* One or both of us specified hmac sha256, but we don't have sha256 module loaded!
		 * We can't allow this password as valid.
		 */
		if (!Instance->FindModule("m_sha256.so") || !Utils->ChallengeResponse)
				return false;
		else
			/* Straight string compare of hashes */
			return ours == theirs;
	}
	else
		/* Straight string compare of plaintext */
		return ours == theirs;
}

bool TreeSocket::Outbound_Reply_Server(std::deque<std::string> &params)
{
	if (params.size() < 4)
		return false;

	irc::string servername = params[0].c_str();
	std::string sname = params[0];
	std::string password = params[1];
	std::string description = params[3];
	int hops = atoi(params[2].c_str());

	this->InboundServerName = sname;
	this->InboundDescription = description;

	if (!sentcapab)
		this->SendCapabilities();

	if (hops)
	{
		this->SendError("Server too far away for authentication");
		this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, server is too far away for authentication");
		return false;
	}

	for (std::vector<Link>::iterator x = Utils->LinkBlocks.begin(); x < Utils->LinkBlocks.end(); x++)
	{
		if ((x->Name == servername) && ((ComparePass(this->MakePass(x->RecvPass,this->GetOurChallenge()),password)) || (x->RecvPass == password && (this->GetTheirChallenge().empty()))))
		{
			TreeServer* CheckDupe = Utils->FindServer(sname);
			if (CheckDupe)
			{
				this->SendError("Server "+sname+" already exists on server "+CheckDupe->GetParent()->GetName()+"!");
				this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, already exists on server "+CheckDupe->GetParent()->GetName());
				return false;
			}
			// Begin the sync here. this kickstarts the
			// other side, waiting in WAIT_AUTH_2 state,
			// into starting their burst, as it shows
			// that we're happy.
			this->LinkState = CONNECTED;
			// we should add the details of this server now
			// to the servers tree, as a child of the root
			// node.
			TreeServer* Node = new TreeServer(this->Utils,this->Instance,sname,description,Utils->TreeRoot,this,x->Hidden);
			Utils->TreeRoot->AddChild(Node);
			params[3] = ":" + params[3];
			Utils->DoOneToAllButSender(Utils->TreeRoot->GetName(),"SERVER",params,sname);
			this->bursting = true;
			this->DoBurst(Node);
			return true;
		}
	}
	this->SendError("Invalid credentials");
	this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, invalid link credentials");
	return false;
}

bool TreeSocket::Inbound_Server(std::deque<std::string> &params)
{
	if (params.size() < 4)
		return false;
	irc::string servername = params[0].c_str();
	std::string sname = params[0];
	std::string password = params[1];
	std::string description = params[3];
	int hops = atoi(params[2].c_str());

	this->InboundServerName = sname;
	this->InboundDescription = description;

	if (!sentcapab)
		this->SendCapabilities();

	if (hops)
	{
		this->SendError("Server too far away for authentication");
		this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, server is too far away for authentication");
		return false;
	}

	for (std::vector<Link>::iterator x = Utils->LinkBlocks.begin(); x < Utils->LinkBlocks.end(); x++)
	{
		if ((x->Name == servername) && ((ComparePass(this->MakePass(x->RecvPass,this->GetOurChallenge()),password) || x->RecvPass == password && (this->GetTheirChallenge().empty()))))
		{
			/* First check for instances of the server that are waiting between the inbound and outbound SERVER command */
			TreeSocket* CheckDupeSocket = Utils->FindBurstingServer(sname);
			if (CheckDupeSocket)
			{
				/* If we find one, we abort the link to prevent a race condition */
				this->SendError("Negotiation collision");
				this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, already exists in a negotiating state.");
				CheckDupeSocket->SendError("Negotiation collision");
				Instance->SE->DelFd(CheckDupeSocket);
				CheckDupeSocket->Close();
				return false;
			}
			/* Now check for fully initialized instances of the server */
			TreeServer* CheckDupe = Utils->FindServer(sname);
			if (CheckDupe)
			{
				this->SendError("Server "+sname+" already exists on server "+CheckDupe->GetParent()->GetName()+"!");
				this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, already exists on server "+CheckDupe->GetParent()->GetName());
				return false;
			}
			this->Instance->SNO->WriteToSnoMask('l',"Verified incoming server connection from \002"+sname+"\002["+(x->HiddenFromStats ? "<hidden>" : this->GetIP())+"] ("+description+")");
			if (this->Hook)
			{
				std::string name = InspSocketNameRequest((Module*)Utils->Creator, this->Hook).Send();
				this->Instance->SNO->WriteToSnoMask('l',"Connection from \2"+sname+"\2["+(x->HiddenFromStats ? "<hidden>" : this->GetIP())+"] using transport \2"+name+"\2");
			}

			Utils->AddBurstingServer(sname,this);

			// this is good. Send our details: Our server name and description and hopcount of 0,
			// along with the sendpass from this block.
			this->WriteLine(std::string("SERVER ")+this->Instance->Config->ServerName+" "+this->MakePass(x->SendPass, this->GetTheirChallenge())+" 0 :"+this->Instance->Config->ServerDesc);
			// move to the next state, we are now waiting for THEM.
			this->LinkState = WAIT_AUTH_2;
			return true;
		}
	}
	this->SendError("Invalid credentials");
	this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, invalid link credentials");
	return false;
}

void TreeSocket::Split(const std::string &line, std::deque<std::string> &n)
{
	n.clear();
	irc::tokenstream tokens(line);
	std::string param;
	while (tokens.GetToken(param))
	{
		if (!param.empty())
			n.push_back(param);
	}
	return;
}

bool TreeSocket::ProcessLine(std::string &line)
{
	std::deque<std::string> params;
	irc::string command;
	std::string prefix;

	line = line.substr(0, line.find_first_of("\r\n"));

	if (line.empty())
		return true;

	Instance->Log(DEBUG, "S[%d] <- %s", this->GetFd(), line.c_str());

	this->Split(line.c_str(),params);
	
	if (params.empty())
		return true;
	
	if ((params[0][0] == ':') && (params.size() > 1))
	{
		prefix = params[0].substr(1);
		params.pop_front();
	}
	command = params[0].c_str();
	params.pop_front();
	switch (this->LinkState)
	{
		TreeServer* Node;

		case WAIT_AUTH_1:
			// Waiting for SERVER command from remote server. Server initiating
			// the connection sends the first SERVER command, listening server
			// replies with theirs if its happy, then if the initiator is happy,
			// it starts to send its net sync, which starts the merge, otherwise
			// it sends an ERROR.
			if (command == "PASS")
			{
				/* Silently ignored */
			}
			else if (command == "SERVER")
			{
				return this->Inbound_Server(params);
			}
			else if (command == "ERROR")
			{
				return this->Error(params);
			}
			else if (command == "USER")
			{
				this->SendError("Client connections to this port are prohibited.");
				return false;
			}
			else if (command == "CAPAB")
			{
				return this->Capab(params);
			}
			else if ((command == "U") || (command == "S"))
			{
				this->SendError("Cannot use the old-style mesh linking protocol with m_spanningtree.so!");
				return false;
			}
			else
			{
				irc::string error = "Invalid command in negotiation phase: " + command;
				this->SendError(assign(error));
				return false;
			}
		break;
		case WAIT_AUTH_2:
			// Waiting for start of other side's netmerge to say they liked our
			// password.
			if (command == "SERVER")
			{
				// cant do this, they sent it to us in the WAIT_AUTH_1 state!
				// silently ignore.
				return true;
			}
			else if ((command == "U") || (command == "S"))
			{
				this->SendError("Cannot use the old-style mesh linking protocol with m_spanningtree.so!");
				return false;
			}
			else if (command == "BURST")
			{
				if (params.size() && Utils->EnableTimeSync)
				{
					bool we_have_delta = (Instance->Time(false) != Instance->Time(true));
					time_t them = atoi(params[0].c_str());
					time_t delta = them - Instance->Time(false);
					if ((delta < -300) || (delta > 300))
					{
						Instance->SNO->WriteToSnoMask('l',"\2ERROR\2: Your clocks are out by %d seconds (this is more than five minutes). Link aborted, \2PLEASE SYNC YOUR CLOCKS!\2",abs(delta));
						SendError("Your clocks are out by "+ConvToStr(abs(delta))+" seconds (this is more than five minutes). Link aborted, PLEASE SYNC YOUR CLOCKS!");
						return false;
					}
					else if ((delta < -30) || (delta > 30))
					{
						Instance->SNO->WriteToSnoMask('l',"\2WARNING\2: Your clocks are out by %d seconds. Please consider synching your clocks.", abs(delta));
					}

					if (!Utils->MasterTime && !we_have_delta)
					{
						this->Instance->SetTimeDelta(delta);
						// Send this new timestamp to any other servers
						Utils->DoOneToMany(Utils->TreeRoot->GetName(), "TIMESET", params);
					}
				}
				this->LinkState = CONNECTED;
				Link* lnk = Utils->FindLink(InboundServerName);
				Node = new TreeServer(this->Utils,this->Instance, InboundServerName, InboundDescription, Utils->TreeRoot, this, lnk ? lnk->Hidden : false);
				Utils->DelBurstingServer(this);
				Utils->TreeRoot->AddChild(Node);
				params.clear();
				params.push_back(InboundServerName);
				params.push_back("*");
				params.push_back("1");
				params.push_back(":"+InboundDescription);
				Utils->DoOneToAllButSender(Utils->TreeRoot->GetName(),"SERVER",params,InboundServerName);
				this->bursting = true;
				this->DoBurst(Node);
			}
			else if (command == "ERROR")
			{
				return this->Error(params);
			}
			else if (command == "CAPAB")
			{
				return this->Capab(params);
			}

		break;
		case LISTENER:
			this->SendError("Internal error -- listening socket accepted its own descriptor!!!");
			return false;
		break;
		case CONNECTING:
			if (command == "SERVER")
			{
				// another server we connected to, which was in WAIT_AUTH_1 state,
				// has just sent us their credentials. If we get this far, theyre
				// happy with OUR credentials, and they are now in WAIT_AUTH_2 state.
				// if we're happy with this, we should send our netburst which
				// kickstarts the merge.
				return this->Outbound_Reply_Server(params);
			}
			else if (command == "ERROR")
			{
				return this->Error(params);
			}
			else if (command == "CAPAB")
			{
				return this->Capab(params);
			}
		break;
		case CONNECTED:
			// This is the 'authenticated' state, when all passwords
			// have been exchanged and anything past this point is taken
			// as gospel.

			if (!prefix.empty())
			{
				std::string direction = prefix;
				userrec* t = this->Instance->FindNick(prefix);
				if (t)
				{
					direction = t->server;
				}
				TreeServer* route_back_again = Utils->BestRouteTo(direction);
				if ((!route_back_again) || (route_back_again->GetSocket() != this))
				{
					if (route_back_again)
						Instance->Log(DEBUG,"Protocol violation: Fake direction in command '%s' from connection '%s'",line.c_str(),this->GetName().c_str());
					return true;
				}
				/* Fix by brain:
				 * When there is activity on the socket, reset the ping counter so
				 * that we're not wasting bandwidth pinging an active server.
				 */
				route_back_again->SetNextPingTime(time(NULL) + 60);
				route_back_again->SetPingFlag();
			}
			else
			{
				prefix = this->GetName();
			}

			if ((command == "MODE") && (params.size() >= 2))
			{
				chanrec* channel = Instance->FindChan(params[0]);
				if (channel)
				{
					userrec* x = Instance->FindNick(prefix);
					if (x)
					{
						if (warned.find(x->server) == warned.end())
						{
							Instance->Log(DEFAULT,"WARNING: I revceived modes '%s' from another server '%s'. This is not compliant with InspIRCd. Please check that server for bugs.", params[1].c_str(), x->server);
							Instance->SNO->WriteToSnoMask('d', "WARNING: The server %s is sending nonstandard modes: '%s MODE %s' where FMODE should be used, and may cause desyncs.", x->server, x->nick, params[1].c_str());
							warned[x->server] = x->nick;
						}
					}
				}
			}

			if (command == "SVSMODE")
			{
				/* Services expects us to implement
				 * SVSMODE. In inspircd its the same as
				 * MODE anyway.
				 */
				command = "MODE";
			}
			std::string target;
			/* Yes, know, this is a mess. Its reasonably fast though as we're
			 * working with std::string here.
			 */
			if ((command == "NICK") && (params.size() >= 8))
			{
				return this->IntroduceClient(prefix,params);
			}
			else if (command == "FJOIN")
			{
				TreeServer* ServerSource = Utils->FindServer(prefix);
				if (ServerSource)
					Utils->SetRemoteBursting(ServerSource, false);
				return this->ForceJoin(prefix,params);
			}
			else if (command == "STATS")
			{
				return this->Stats(prefix, params);
			}
			else if (command == "MOTD")
			{
				return this->Motd(prefix, params);
			}
			else if (command == "KILL" && Utils->IsServer(prefix))
			{
				return this->RemoteKill(prefix,params);
			}
			else if (command == "MODULES")
			{
				return this->Modules(prefix, params);
			}
			else if (command == "ADMIN")
			{
				return this->Admin(prefix, params);
			}
			else if (command == "SERVER")
			{
				return this->RemoteServer(prefix,params);
			}
			else if (command == "ERROR")
			{
				return this->Error(params);
			}
			else if (command == "OPERTYPE")
			{
				return this->OperType(prefix,params);
			}
			else if (command == "FMODE")
			{
				TreeServer* ServerSource = Utils->FindServer(prefix);
				if (ServerSource)
					Utils->SetRemoteBursting(ServerSource, false);
				return this->ForceMode(prefix,params);
			}
			else if (command == "FTOPIC")
			{
				return this->ForceTopic(prefix,params);
			}
			else if (command == "REHASH")
			{
				return this->RemoteRehash(prefix,params);
			}
			else if (command == "METADATA")
			{
				return this->MetaData(prefix,params);
			}
			else if (command == "REMSTATUS")
			{
				return this->RemoveStatus(prefix,params);
			}
			else if (command == "PING")
			{
				if (prefix.empty())
					prefix = this->GetName();
				/*
				 * We just got a ping from a server that's bursting.
				 * This can't be right, so set them to not bursting, and
				 * apply their lines.
				 */
				TreeServer* ServerSource = Utils->FindServer(prefix);
				if (ServerSource)
					Utils->SetRemoteBursting(ServerSource, false);

				if (this->bursting)
				{
					this->bursting = false;
					Instance->XLines->apply_lines(Utils->lines_to_apply);
					Utils->lines_to_apply = 0;
				}

				return this->LocalPing(prefix,params);
			}
			else if (command == "PONG")
			{
				if (prefix.empty())
					prefix = this->GetName();
				/*
				 * We just got a pong from a server that's bursting.
				 * This can't be right, so set them to not bursting, and
				 * apply their lines.
				 */
				TreeServer* ServerSource = Utils->FindServer(prefix);
				if (ServerSource)
					Utils->SetRemoteBursting(ServerSource, false);

				if (this->bursting)
				{
					this->bursting = false;
					Instance->XLines->apply_lines(Utils->lines_to_apply);
					Utils->lines_to_apply = 0;
				}

				return this->LocalPong(prefix,params);
			}
			else if (command == "VERSION")
			{
				return this->ServerVersion(prefix,params);
			}
			else if (command == "FHOST")
			{
				return this->ChangeHost(prefix,params);
			}
			else if (command == "FNAME")
			{
				return this->ChangeName(prefix,params);
			}
			else if (command == "ADDLINE")
			{
				TreeServer* ServerSource = Utils->FindServer(prefix);
				if (ServerSource)
					Utils->SetRemoteBursting(ServerSource, false);
				return this->AddLine(prefix,params);
			}
			else if (command == "SVSNICK")
			{
				if (prefix.empty())
				{
					prefix = this->GetName();
				}
				return this->ForceNick(prefix,params);
			}
			else if (command == "OPERQUIT")
			{
				return this->OperQuit(prefix,params);
			}
			else if (command == "IDLE")
			{
				return this->Whois(prefix,params);
			}
			else if (command == "PUSH")
			{
				return this->Push(prefix,params);
			}
			else if (command == "TIMESET")
			{
				return this->HandleSetTime(prefix, params);
			}
			else if (command == "TIME")
			{
				return this->Time(prefix,params);
			}
			else if ((command == "KICK") && (Utils->IsServer(prefix)))
			{
				std::string sourceserv = this->myhost;
				if (params.size() == 3)
				{
					userrec* user = this->Instance->FindNick(params[1]);
					chanrec* chan = this->Instance->FindChan(params[0]);
					if (user && chan)
					{
						if (!chan->ServerKickUser(user, params[2].c_str(), false))
							/* Yikes, the channels gone! */
							delete chan;
					}
				}
				if (!this->InboundServerName.empty())
				{
					sourceserv = this->InboundServerName;
				}
				return Utils->DoOneToAllButSenderRaw(line,sourceserv,prefix,command,params);
			}
			else if (command == "SVSJOIN")
			{
				if (prefix.empty())
				{
					prefix = this->GetName();
				}
				return this->ServiceJoin(prefix,params);
			}
			else if (command == "SQUIT")
			{
				if (params.size() == 2)
				{
					this->Squit(Utils->FindServer(params[0]),params[1]);
				}
				return true;
			}
			else if (command == "OPERNOTICE")
			{
				std::string sourceserv = this->myhost;
				if (!this->InboundServerName.empty())
					sourceserv = this->InboundServerName;
				if (params.size() >= 1)
					Instance->WriteOpers("*** From " + sourceserv + ": " + params[0]);
				return Utils->DoOneToAllButSenderRaw(line, sourceserv, prefix, command, params);
			}
			else if (command == "MODENOTICE")
			{
				std::string sourceserv = this->myhost;
				if (!this->InboundServerName.empty())
					sourceserv = this->InboundServerName;
				if (params.size() >= 2)
				{
					Instance->WriteMode(params[0].c_str(), WM_AND, "*** From %s: %s", sourceserv.c_str(), params[1].c_str());
				}
				return Utils->DoOneToAllButSenderRaw(line, sourceserv, prefix, command, params);
			}
			else if (command == "SNONOTICE")
			{
				std::string sourceserv = this->myhost;
				if (!this->InboundServerName.empty())
					sourceserv = this->InboundServerName;
				if (params.size() >= 2)
				{
					Instance->SNO->WriteToSnoMask(*(params[0].c_str()), "From " + sourceserv + ": "+ params[1]);
				}
				return Utils->DoOneToAllButSenderRaw(line, sourceserv, prefix, command, params);
			}
			else if (command == "ENDBURST")
			{
				this->bursting = false;
				Instance->XLines->apply_lines(Utils->lines_to_apply);
				Utils->lines_to_apply = 0;
				std::string sourceserv = this->myhost;
				if (!this->InboundServerName.empty())
					sourceserv = this->InboundServerName;
				this->Instance->SNO->WriteToSnoMask('l',"Received end of netburst from \2%s\2",sourceserv.c_str());

				Event rmode((char*)sourceserv.c_str(), (Module*)Utils->Creator, "new_server");
				rmode.Send(Instance);

				return true;
			}
			else
			{
				// not a special inter-server command.
				// Emulate the actual user doing the command,
				// this saves us having a huge ugly parser.
				userrec* who = this->Instance->FindNick(prefix);
				std::string sourceserv = this->myhost;
				if (!this->InboundServerName.empty())
				{
					sourceserv = this->InboundServerName;
				}
				if ((!who) && (command == "MODE"))
				{
					if (Utils->IsServer(prefix))
					{
						const char* modelist[127];
						for (size_t i = 0; i < params.size(); i++)
							modelist[i] = params[i].c_str();
						userrec* fake = new userrec(Instance);
						fake->SetFd(FD_MAGIC_NUMBER);
						this->Instance->SendMode(modelist, params.size(), fake);

						delete fake;
						/* Hot potato! pass it on! */
						return Utils->DoOneToAllButSenderRaw(line,sourceserv,prefix,command,params);
					}
				}
				if (who)
				{
					if ((command == "NICK") && (params.size() > 0))
					{
						/* On nick messages, check that the nick doesnt
						 * already exist here. If it does, kill their copy,
						 * and our copy.
						 */
						userrec* x = this->Instance->FindNick(params[0]);
						if ((x) && (x != who))
						{
							std::deque<std::string> p;
							p.push_back(params[0]);
							p.push_back("Nickname collision ("+prefix+" -> "+params[0]+")");
							Utils->DoOneToMany(this->Instance->Config->ServerName,"KILL",p);
							p.clear();
							p.push_back(prefix);
							p.push_back("Nickname collision");
							Utils->DoOneToMany(this->Instance->Config->ServerName,"KILL",p);
							userrec::QuitUser(this->Instance,x,"Nickname collision ("+prefix+" -> "+params[0]+")");
							userrec* y = this->Instance->FindNick(prefix);
							if (y)
							{
								userrec::QuitUser(this->Instance,y,"Nickname collision");
							}
							return Utils->DoOneToAllButSenderRaw(line,sourceserv,prefix,command,params);
						}
					}
					// its a user
					target = who->server;
					const char* strparams[127];
					for (unsigned int q = 0; q < params.size(); q++)
					{
						strparams[q] = params[q].c_str();
					}
					switch (this->Instance->CallCommandHandler(command.c_str(), strparams, params.size(), who))
					{
						case CMD_INVALID:
							this->SendError("Unrecognised command '"+std::string(command.c_str())+"' -- possibly loaded mismatched modules");
							return false;
						break;
						case CMD_FAILURE:
							return true;
						break;
						default:
							/* CMD_SUCCESS and CMD_USER_DELETED fall through here */
						break;
					}
				}
				else
				{
					// its not a user. Its either a server, or somethings screwed up.
					if (Utils->IsServer(prefix))
						target = this->Instance->Config->ServerName;
					else
						return true;
				}
				return Utils->DoOneToAllButSenderRaw(line,sourceserv,prefix,command,params);

			}
			return true;
		break;
	}
	return true;
}

std::string TreeSocket::GetName()
{
	std::string sourceserv = this->myhost;
	if (!this->InboundServerName.empty())
	{
		sourceserv = this->InboundServerName;
	}
	return sourceserv;
}

void TreeSocket::OnTimeout()
{
	if (this->LinkState == CONNECTING)
	{
		this->Instance->SNO->WriteToSnoMask('l',"CONNECT: Connection to \002"+myhost+"\002 timed out.");
		Link* MyLink = Utils->FindLink(myhost);
		if (MyLink)
			Utils->DoFailOver(MyLink);
	}
}

void TreeSocket::OnClose()
{
	if (this->LinkState == LISTENER)
		return;

	// Connection closed.
	// If the connection is fully up (state CONNECTED)
	// then propogate a netsplit to all peers.
	std::string quitserver = this->myhost;
	if (!this->InboundServerName.empty())
	{
		quitserver = this->InboundServerName;
	}
	TreeServer* s = Utils->FindServer(quitserver);
	if (s)
	{
		Squit(s,"Remote host closed the connection");
	}

	if (!quitserver.empty())
	{
		this->Instance->SNO->WriteToSnoMask('l',"Connection to '\2%s\2' failed.",quitserver.c_str());
		time_t server_uptime = Instance->Time() - this->age;	
		if (server_uptime)
			Instance->SNO->WriteToSnoMask('l',"Connection to '\2%s\2' was established for %s", quitserver.c_str(), Utils->Creator->TimeToStr(server_uptime).c_str());
	}
}

int TreeSocket::OnIncomingConnection(int newsock, char* ip)
{
	/* To prevent anyone from attempting to flood opers/DDoS by connecting to the server port,
	 * or discovering if this port is the server port, we don't allow connections from any
	 * IPs for which we don't have a link block.
	 */
	bool found = false;

	found = (std::find(Utils->ValidIPs.begin(), Utils->ValidIPs.end(), ip) != Utils->ValidIPs.end());
	if (!found)
	{
		for (vector<std::string>::iterator i = Utils->ValidIPs.begin(); i != Utils->ValidIPs.end(); i++)
			if (irc::sockets::MatchCIDR(ip, (*i).c_str()))
				found = true;

		if (!found)
		{
			this->Instance->SNO->WriteToSnoMask('l',"Server connection from %s denied (no link blocks with that IP address)", ip);
			close(newsock);
			return false;
		}
	}

	TreeSocket* s = new TreeSocket(this->Utils, this->Instance, newsock, ip, this->Hook);
	s = s; /* Whinge whinge whinge, thats all GCC ever does. */
	return true;
}
