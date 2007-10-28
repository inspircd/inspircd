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

/* $Core: libIRCDxline */

#include "inspircd.h"
#include "wildcard.h"
#include "xline.h"

/*
 * This is now version 3 of the XLine subsystem, let's see if we can get it as nice and 
 * efficient as we can this time so we can close this file and never ever touch it again ..
 *
 * Background:
 *  Version 1 stored all line types in one list (one for g, one for z, etc). This was fine,
 *  but both version 1 and 2 suck at applying lines efficiently. That is, every time a new line
 *  was added, it iterated every existing line for every existing user. Ow. Expiry was also
 *  expensive, as the lists were NOT sorted.
 *
 *  Version 2 moved permanent lines into a seperate list from non-permanent to help optimize
 *  matching speed, but matched in the same way.
 *  Expiry was also sped up by sorting the list by expiry (meaning just remove the items at the
 *  head of the list that are outdated.)
 *
 * This was fine and good, but it looked less than ideal in code, and matching was still slower
 * than it could have been, something which we address here.
 *
 * VERSION 3:
 *  All lines are (as in v1) stored together -- no seperation of perm and non-perm. Expiry will
 *  still use a sorted list, and we'll just ignore anything permanent.
 *
 *  Application will be by a list of lines 'pending' application, meaning only the newly added lines
 *  will be gone over. Much faster.
 *
 * More of course is to come.
 */

/* Version two, now with optimized expiry!
 *
 * Because the old way was horrendously slow, the new way of expiring xlines is very
 * very efficient. I have improved the efficiency of the algorithm in two ways:
 *
 * (1) There are now two lists of items for each linetype. One list holds temporary
 *     items, and the other list holds permanent items (ones which will expire).
 *     Items which are on the permanent list are NEVER checked at all by the
 *     expire_lines() function.
 * (2) The temporary xline lists are always kept in strict numerical order, keyed by
 *     current time + duration. This means that the line which is due to expire the
 *     soonest is always pointed at by vector::begin(), so a simple while loop can
 *     very efficiently, very quickly and above all SAFELY pick off the first few
 *     items in the vector which need zapping.
 *
 *     -- Brain
 */

bool InitXLine(ServerConfig* conf, const char* tag)
{
	return true;
}

bool DoneZLine(ServerConfig* conf, const char* tag)
{
	// XXX we should really only call this once - after we've finished processing configuration all together
	conf->GetInstance()->XLines->ApplyLines();
	return true;
}

bool DoneQLine(ServerConfig* conf, const char* tag)
{
	// XXX we should really only call this once - after we've finished processing configuration all together
	conf->GetInstance()->XLines->ApplyLines();
	return true;
}

bool DoneKLine(ServerConfig* conf, const char* tag)
{
	// XXX we should really only call this once - after we've finished processing configuration all together
	conf->GetInstance()->XLines->ApplyLines();
	return true;
}

bool DoneELine(ServerConfig* conf, const char* tag)
{
	// XXX we should really only call this once - after we've finished processing configuration all together
	conf->GetInstance()->XLines->ApplyLines();
	return true;
}

bool DoZLine(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types)
{
	const char* reason = values[0].GetString();
	const char* ipmask = values[1].GetString();

	conf->GetInstance()->XLines->AddZLine(0,"<Config>",reason,ipmask);
	return true;
}

bool DoQLine(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types)
{
	const char* reason = values[0].GetString();
	const char* nick = values[1].GetString();

	conf->GetInstance()->XLines->AddQLine(0,"<Config>",reason,nick);
	return true;
}

bool DoKLine(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types)
{
	const char* reason = values[0].GetString();
	const char* host = values[1].GetString();

	conf->GetInstance()->XLines->AddKLine(0,"<Config>",reason,host);
	return true;
}

bool DoELine(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types)
{
	const char* reason = values[0].GetString();
	const char* host = values[1].GetString();

	conf->GetInstance()->XLines->AddELine(0,"<Config>",reason,host);
	return true;
}

bool XLine::Matches(User *u)
{
	return false;
}

IdentHostPair XLineManager::IdentSplit(const std::string &ident_and_host)
{
	IdentHostPair n = std::make_pair<std::string,std::string>("*","*");
	std::string::size_type x = ident_and_host.find('@');
	if (x != std::string::npos)
	{
		n.second = ident_and_host.substr(x + 1,ident_and_host.length());
		n.first = ident_and_host.substr(0, x);
		if (!n.first.length())
			n.first.assign("*");
		if (!n.second.length())
			n.second.assign("*");
	}
	else
	{
		n.second = ident_and_host;
	}

	return n;
}

// adds a g:line

bool XLineManager::AddGLine(long duration, const char* source,const char* reason,const char* hostmask)
{
	IdentHostPair ih = IdentSplit(hostmask);

	if (DelGLine(hostmask, true))
		return false;

	GLine* item = new GLine(ServerInstance->Time(), duration, source, reason, ih.first.c_str(), ih.second.c_str());

	glines.push_back(item);
	sort(glines.begin(), glines.end(),XLineManager::XSortComparison);

	return true;
}

// adds an e:line (exception to bans)

bool XLineManager::AddELine(long duration, const char* source, const char* reason, const char* hostmask)
{
	IdentHostPair ih = IdentSplit(hostmask);

	if (DelELine(hostmask, true))
		return false;

	ELine* item = new ELine(ServerInstance->Time(), duration, source, reason, ih.first.c_str(), ih.second.c_str());

	elines.push_back(item);
	sort(elines.begin(), elines.end(),XLineManager::XSortComparison);

	return true;
}

// adds a q:line

bool XLineManager::AddQLine(long duration, const char* source, const char* reason, const char* nickname)
{
	if (DelQLine(nickname, true))
		return false;

	QLine* item = new QLine(ServerInstance->Time(), duration, source, reason, nickname);

	qlines.push_back(item);
	sort(qlines.begin(), qlines.end(),XLineManager::XSortComparison);

	return true;
}

// adds a z:line

bool XLineManager::AddZLine(long duration, const char* source, const char* reason, const char* ipaddr)
{
	if (strchr(ipaddr,'@'))
	{
		while (*ipaddr != '@')
			ipaddr++;
		ipaddr++;
	}

	if (DelZLine(ipaddr, true))
		return false;

	ZLine* item = new ZLine(ServerInstance->Time(), duration, source, reason, ipaddr);

	zlines.push_back(item);
	sort(zlines.begin(), zlines.end(),XLineManager::XSortComparison);

	return true;
}

// adds a k:line

bool XLineManager::AddKLine(long duration, const char* source, const char* reason, const char* hostmask)
{
	IdentHostPair ih = IdentSplit(hostmask);

	if (DelKLine(hostmask, true))
		return false;

	KLine* item = new KLine(ServerInstance->Time(), duration, source, reason, ih.first.c_str(), ih.second.c_str());

	klines.push_back(item);
	sort(klines.begin(), klines.end(),XLineManager::XSortComparison);

	return true;
}

// deletes a g:line, returns true if the line existed and was removed

bool XLineManager::DelGLine(const char* hostmask, bool simulate)
{
	IdentHostPair ih = IdentSplit(hostmask);
	for (std::vector<GLine*>::iterator i = glines.begin(); i != glines.end(); i++)
	{
		if (!strcasecmp(ih.first.c_str(),(*i)->identmask) && !strcasecmp(ih.second.c_str(),(*i)->hostmask))
		{
			if (!simulate)
			{
				delete *i;
				glines.erase(i);
			}
			return true;
		}
	}

	return false;
}

// deletes a e:line, returns true if the line existed and was removed

bool XLineManager::DelELine(const char* hostmask, bool simulate)
{
	IdentHostPair ih = IdentSplit(hostmask);
	for (std::vector<ELine*>::iterator i = elines.begin(); i != elines.end(); i++)
	{
		if (!strcasecmp(ih.first.c_str(),(*i)->identmask) && !strcasecmp(ih.second.c_str(),(*i)->hostmask))
		{
			if (!simulate)
			{
				delete *i;
				elines.erase(i);
			}
			return true;
		}
	}

	return false;
}

// deletes a q:line, returns true if the line existed and was removed

bool XLineManager::DelQLine(const char* nickname, bool simulate)
{
	for (std::vector<QLine*>::iterator i = qlines.begin(); i != qlines.end(); i++)
	{
		if (!strcasecmp(nickname,(*i)->nick))
		{
			if (!simulate)
			{
				delete *i;
				qlines.erase(i);
			}
			return true;
		}
	}

	return false;
}

// deletes a z:line, returns true if the line existed and was removed

bool XLineManager::DelZLine(const char* ipaddr, bool simulate)
{
	for (std::vector<ZLine*>::iterator i = zlines.begin(); i != zlines.end(); i++)
	{
		if (!strcasecmp(ipaddr,(*i)->ipaddr))
		{
			if (!simulate)
			{
				delete *i;
				zlines.erase(i);
			}
			return true;
		}
	}

	return false;
}

// deletes a k:line, returns true if the line existed and was removed

bool XLineManager::DelKLine(const char* hostmask, bool simulate)
{
	IdentHostPair ih = IdentSplit(hostmask);
	for (std::vector<KLine*>::iterator i = klines.begin(); i != klines.end(); i++)
	{
		if (!strcasecmp(ih.first.c_str(),(*i)->identmask) && !strcasecmp(ih.second.c_str(),(*i)->hostmask))
		{
			if (!simulate)
			{
				delete *i;
				klines.erase(i);
			}
			return true;
		}
	}

	return false;
}

// returns a pointer to the reason if a nickname matches a qline, NULL if it didnt match

QLine* XLineManager::matches_qline(const char* nick)
{
	if (qlines.empty())
		return NULL;

	for (std::vector<QLine*>::iterator i = qlines.begin(); i != qlines.end(); i++)
		if (match(nick,(*i)->nick))
			return (*i);
	return NULL;
}

// returns a pointer to the reason if a host matches a gline, NULL if it didnt match

GLine* XLineManager::matches_gline(User* user)
{
	if (glines.empty())
		return NULL;

	for (std::vector<GLine*>::iterator i = glines.begin(); i != glines.end(); i++)
	{
		if ((match(user->ident,(*i)->identmask)))
		{
			if ((match(user->host,(*i)->hostmask, true)) || (match(user->GetIPString(),(*i)->hostmask, true)))
			{
				return (*i);
			}
		}
	}

	return NULL;
}

ELine* XLineManager::matches_exception(User* user)
{
	if (elines.empty())
		return NULL;
	char host2[MAXBUF];

	snprintf(host2,MAXBUF,"*@%s",user->host);
	for (std::vector<ELine*>::iterator i = elines.begin(); i != elines.end(); i++)
	{
		if ((match(user->ident,(*i)->identmask)))
		{
			if ((match(user->host,(*i)->hostmask, true)) || (match(user->GetIPString(),(*i)->hostmask, true)))
			{
				return (*i);
			}
		}
	}
	return NULL;
}


void XLineManager::gline_set_creation_time(const char* host, time_t create_time)
{
	for (std::vector<GLine*>::iterator i = glines.begin(); i != glines.end(); i++)
	{
		if (!strcasecmp(host,(*i)->hostmask))
		{
			(*i)->set_time = create_time;
			(*i)->expiry = create_time + (*i)->duration;
			return;
		}
	}

	return ;
}

void XLineManager::eline_set_creation_time(const char* host, time_t create_time)
{
	for (std::vector<ELine*>::iterator i = elines.begin(); i != elines.end(); i++)
	{
		if (!strcasecmp(host,(*i)->hostmask))
		{
			(*i)->set_time = create_time;
			(*i)->expiry = create_time + (*i)->duration;
			return;
		}
	}

	return;
}

void XLineManager::qline_set_creation_time(const char* nick, time_t create_time)
{
	for (std::vector<QLine*>::iterator i = qlines.begin(); i != qlines.end(); i++)
	{
		if (!strcasecmp(nick,(*i)->nick))
		{
			(*i)->set_time = create_time;
			(*i)->expiry = create_time + (*i)->duration;
			return;
		}
	}

	return;
}

void XLineManager::zline_set_creation_time(const char* ip, time_t create_time)
{
	for (std::vector<ZLine*>::iterator i = zlines.begin(); i != zlines.end(); i++)
	{
		if (!strcasecmp(ip,(*i)->ipaddr))
		{
			(*i)->set_time = create_time;
			(*i)->expiry = create_time + (*i)->duration;
			return;
		}
	}

	return;
}

// returns a pointer to the reason if an ip address matches a zline, NULL if it didnt match

ZLine* XLineManager::matches_zline(const char* ipaddr)
{
	if (zlines.empty())
		return NULL;

	for (std::vector<ZLine*>::iterator i = zlines.begin(); i != zlines.end(); i++)
		if (match(ipaddr,(*i)->ipaddr, true))
			return (*i);
	return NULL;
}

// returns a pointer to the reason if a host matches a kline, NULL if it didnt match

KLine* XLineManager::matches_kline(User* user)
{
	if (klines.empty())
		return NULL;

	for (std::vector<KLine*>::iterator i = klines.begin(); i != klines.end(); i++)
	{
		if ((*i)->Matches(user))
			return (*i);
	}

	return NULL;
}

bool XLineManager::XSortComparison(const XLine *one, const XLine *two)
{
	// account for permanent lines
	if (one->expiry == 0)
	{
		return false;
	}
	return (one->expiry) < (two->expiry);
}

// removes lines that have expired
void XLineManager::expire_lines()
{
	time_t current = ServerInstance->Time();

	/* Because we now store all our XLines in sorted order using ((*i)->duration + (*i)->set_time) as a key, this
	 * means that to expire the XLines we just need to do a while, picking off the top few until there are
	 * none left at the head of the queue that are after the current time.
	 */

	while ((glines.size()) && (current > (*glines.begin())->expiry) && ((*glines.begin())->duration != 0))
	{
		std::vector<GLine*>::iterator i = glines.begin();
		ServerInstance->SNO->WriteToSnoMask('x',"Expiring timed G-Line %s@%s (set by %s %d seconds ago)",(*i)->identmask,(*i)->hostmask,(*i)->source,(*i)->duration);
		glines.erase(i);
	}

	while ((elines.size()) && (current > (*elines.begin())->expiry) && ((*elines.begin())->duration != 0))
	{
		std::vector<ELine*>::iterator i = elines.begin();
		ServerInstance->SNO->WriteToSnoMask('x',"Expiring timed E-Line %s@%s (set by %s %d seconds ago)",(*i)->identmask,(*i)->hostmask,(*i)->source,(*i)->duration);
		elines.erase(i);
	}

	while ((zlines.size()) && (current > (*zlines.begin())->expiry) && ((*zlines.begin())->duration != 0))
	{
		std::vector<ZLine*>::iterator i = zlines.begin();
		ServerInstance->SNO->WriteToSnoMask('x',"Expiring timed Z-Line %s (set by %s %d seconds ago)",(*i)->ipaddr,(*i)->source,(*i)->duration);
		zlines.erase(i);
	}

	while ((klines.size()) && (current > (*klines.begin())->expiry) && ((*klines.begin())->duration != 0))
	{
		std::vector<KLine*>::iterator i = klines.begin();
		ServerInstance->SNO->WriteToSnoMask('x',"Expiring timed K-Line %s@%s (set by %s %d seconds ago)",(*i)->identmask,(*i)->hostmask,(*i)->source,(*i)->duration);
		klines.erase(i);
	}

	while ((qlines.size()) && (current > (*qlines.begin())->expiry) && ((*qlines.begin())->duration != 0))
	{
		std::vector<QLine*>::iterator i = qlines.begin();
		ServerInstance->SNO->WriteToSnoMask('x',"Expiring timed Q-Line %s (set by %s %d seconds ago)",(*i)->nick,(*i)->source,(*i)->duration);
		qlines.erase(i);
	}

}

// applies lines, removing clients and changing nicks etc as applicable

void XLineManager::ApplyLines()
{
	int What = 0; // XXX remove me
	char reason[MAXBUF];

	XLine* check = NULL;
	for (std::vector<User*>::const_iterator u2 = ServerInstance->local_users.begin(); u2 != ServerInstance->local_users.end(); u2++)
	{
		User* u = (User*)(*u2);

		if (elines.size())
		{
			// ignore people matching exempts
			if (matches_exception(u))
				continue;
		}
		if ((What & APPLY_GLINES) && (glines.size()))
		{
			if ((check = matches_gline(u)))
			{
				snprintf(reason,MAXBUF,"G-Lined: %s",check->reason);
				if (*ServerInstance->Config->MoronBanner)
					u->WriteServ("NOTICE %s :*** %s", u->nick, ServerInstance->Config->MoronBanner);
				if (ServerInstance->Config->HideBans)
					User::QuitUser(ServerInstance, u, "G-Lined", reason);
				else
					User::QuitUser(ServerInstance, u, reason);
			}
		}
		if ((What & APPLY_KLINES) && (klines.size()))
		{
			if ((check = matches_kline(u)))
			{
				snprintf(reason,MAXBUF,"K-Lined: %s",check->reason);
				if (*ServerInstance->Config->MoronBanner)
					u->WriteServ("NOTICE %s :*** %s", u->nick, ServerInstance->Config->MoronBanner);
				if (ServerInstance->Config->HideBans)
					User::QuitUser(ServerInstance, u, "K-Lined", reason);
				else
					User::QuitUser(ServerInstance, u, reason);
			}
		}
		if ((What & APPLY_QLINES) && (qlines.size()))
		{
			if ((check = matches_qline(u->nick)))
			{
				snprintf(reason,MAXBUF,"Q-Lined: %s",check->reason);
				if (*ServerInstance->Config->MoronBanner)
					u->WriteServ("NOTICE %s :*** %s", u->nick, ServerInstance->Config->MoronBanner);
				if (ServerInstance->Config->HideBans)
					User::QuitUser(ServerInstance, u, "Q-Lined", reason);
				else
					User::QuitUser(ServerInstance, u, reason);
			}
		}
		if ((What & APPLY_ZLINES) && (zlines.size()))
		{
			if ((check = matches_zline(u->GetIPString())))
			{
				snprintf(reason,MAXBUF,"Z-Lined: %s", check->reason);
				if (*ServerInstance->Config->MoronBanner)
					u->WriteServ("NOTICE %s :*** %s", u->nick, ServerInstance->Config->MoronBanner);
				if (ServerInstance->Config->HideBans)
					User::QuitUser(ServerInstance, u, "Z-Lined", reason);
				else
					User::QuitUser(ServerInstance, u, reason);
			}
		}
	}
}

void XLineManager::stats_k(User* user, string_list &results)
{
	std::string sn = ServerInstance->Config->ServerName;
	for (std::vector<KLine*>::iterator i = klines.begin(); i != klines.end(); i++)
		results.push_back(sn+" 216 "+user->nick+" :"+(*i)->identmask+"@"+(*i)->hostmask+" "+ConvToStr((*i)->set_time)+" "+ConvToStr((*i)->duration)+" "+(*i)->source+" :"+(*i)->reason);
}

void XLineManager::stats_g(User* user, string_list &results)
{
	std::string sn = ServerInstance->Config->ServerName;
	for (std::vector<GLine*>::iterator i = glines.begin(); i != glines.end(); i++)
		results.push_back(sn+" 223 "+user->nick+" :"+(*i)->identmask+"@"+(*i)->hostmask+" "+ConvToStr((*i)->set_time)+" "+ConvToStr((*i)->duration)+" "+(*i)->source+" :"+(*i)->reason);
}

void XLineManager::stats_q(User* user, string_list &results)
{
	std::string sn = ServerInstance->Config->ServerName;
	for (std::vector<QLine*>::iterator i = qlines.begin(); i != qlines.end(); i++)
		results.push_back(sn+" 217 "+user->nick+" :"+(*i)->nick+" "+ConvToStr((*i)->set_time)+" "+ConvToStr((*i)->duration)+" "+(*i)->source+" :"+(*i)->reason);
}

void XLineManager::stats_z(User* user, string_list &results)
{
	std::string sn = ServerInstance->Config->ServerName;
	for (std::vector<ZLine*>::iterator i = zlines.begin(); i != zlines.end(); i++)
		results.push_back(sn+" 223 "+user->nick+" :"+(*i)->ipaddr+" "+ConvToStr((*i)->set_time)+" "+ConvToStr((*i)->duration)+" "+(*i)->source+" :"+(*i)->reason);
}

void XLineManager::stats_e(User* user, string_list &results)
{
	std::string sn = ServerInstance->Config->ServerName;
	for (std::vector<ELine*>::iterator i = elines.begin(); i != elines.end(); i++)
		results.push_back(sn+" 223 "+user->nick+" :"+(*i)->identmask+"@"+(*i)->hostmask+" "+ConvToStr((*i)->set_time)+" "+ConvToStr((*i)->duration)+" "+(*i)->source+" :"+(*i)->reason);
}

XLineManager::XLineManager(InspIRCd* Instance) : ServerInstance(Instance)
{
}



bool KLine::Matches(User *u)
{
	if ((match(u->ident, this->identmask)))
	{
		if ((match(u->host, this->hostmask, true)) || (match(u->GetIPString(), this->hostmask, true)))
		{
			return true;
		}
	}

	return false;
}

bool GLine::Matches(User *u)
{
	return false;
}

bool ELine::Matches(User *u)
{
	return false;
}

bool ZLine::Matches(User *u)
{
	if (match(user->GetIPString(), this->ipaddr, true))
		return true;
	else
		return false;
}

bool QLine::Matches(User *u)
{
	return false;
}
