/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
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

bool XLine::Matches(User *u)
{
	return false;
}

/*
 * Checks what users match a given vector of ELines and sets their ban exempt flag accordingly.
 */
void XLineManager::CheckELines(std::map<std::string, XLine *> &ELines)
{
	if (ELines.empty())
		return;

	for (std::vector<User*>::const_iterator u2 = ServerInstance->local_users.begin(); u2 != ServerInstance->local_users.end(); u2++)
	{
		User* u = (User*)(*u2);

		for (std::map<std::string, XLine *>::iterator i = ELines.begin(); i != ELines.end(); i++)
		{
			XLine *e = i->second;
			u->exempt = e->Matches(u);
		}
	}
}

// this should probably be moved to configreader, but atm it relies on CheckELines above.
bool DoneELine(ServerConfig* conf, const char* tag)
{
	for (std::vector<User*>::const_iterator u2 = conf->GetInstance()->local_users.begin(); u2 != conf->GetInstance()->local_users.end(); u2++)
	{
		User* u = (User*)(*u2);
		u->exempt = false;
	}

	conf->GetInstance()->XLines->CheckELines(conf->GetInstance()->XLines->lookup_lines['E']);
	return true;
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

/*bool XLineManager::AddELine(long duration, const char* source, const char* reason, const char* hostmask)*/
bool XLineManager::AddLine(XLine* line, User* user)
{
	/*IdentHostPair ih = IdentSplit(hostmask);*/

	if (DelLine(line->Displayable(), line->type, user, true))
		return false;

	/*ELine* item = new ELine(ServerInstance, ServerInstance->Time(), duration, source, reason, ih.first.c_str(), ih.second.c_str());*/

	active_lines.push_back(line);
	sort(active_lines.begin(), active_lines.end(), XLineManager::XSortComparison);
	pending_lines.push_back(line);
	lookup_lines[line->type][line->Displayable()] = line;
	line->OnAdd();

	if (!line->duration)
		PermLines++;

	FOREACH_MOD(I_OnAddLine,OnAddLine(user, line));	

	return true;
}

/*bool XLineManager::AddZLine(long duration, const char* source, const char* reason, const char* ipaddr)
{
	if (strchr(ipaddr,'@'))
	{
		while (*ipaddr != '@')
			ipaddr++;
		ipaddr++;
	}*/

// deletes a g:line, returns true if the line existed and was removed

bool XLineManager::DelLine(const char* hostmask, char type, User* user, bool simulate)
{
	IdentHostPair ih = IdentSplit(hostmask);
	for (std::vector<XLine*>::iterator i = active_lines.begin(); i != active_lines.end(); i++)
	{
		if ((*i)->type == type)
		{
			if ((*i)->MatchesLiteral(hostmask))
			{
				if (!simulate)
				{
					(*i)->Unset();

					if (lookup_lines.find(type) != lookup_lines.end())
						lookup_lines[type].erase(hostmask);

					FOREACH_MOD(I_OnDelLine,OnDelLine(user, *i));

					std::vector<XLine*>::iterator pptr = std::find(pending_lines.begin(), pending_lines.end(), *i);					
					if (pptr != pending_lines.end())
						pending_lines.erase(pptr);

					if (!(*i)->duration)
						PermLines--;

					delete *i;
					active_lines.erase(i);
				}
				return true;
			}
		}
	}

	return false;
}


void ELine::Unset()
{
	/* remove exempt from everyone and force recheck after deleting eline */
	for (std::vector<User*>::const_iterator u2 = ServerInstance->local_users.begin(); u2 != ServerInstance->local_users.end(); u2++)
	{
		User* u = (User*)(*u2);
		u->exempt = false;
	}

	if (ServerInstance->XLines->lookup_lines.find('E') != ServerInstance->XLines->lookup_lines.end())
		ServerInstance->XLines->CheckELines(ServerInstance->XLines->lookup_lines['E']);
}

// returns a pointer to the reason if a nickname matches a qline, NULL if it didnt match

QLine* XLineManager::matches_qline(const char* nick)
{
	if (lookup_lines.find('Q') == lookup_lines.end())
		return NULL;

	if (lookup_lines.find('Q') != lookup_lines.end() && lookup_lines['Q'].empty())
		return NULL;

	for (std::vector<XLine*>::iterator i = active_lines.begin(); i != active_lines.end(); i++)
		if ((*i)->type == 'Q' && (*i)->Matches(nick))
			return (QLine*)(*i);
	return NULL;
}

// returns a pointer to the reason if a host matches a gline, NULL if it didnt match

GLine* XLineManager::matches_gline(User* user)
{
	if (lookup_lines.find('G') == lookup_lines.end())
		return NULL;

	if (lookup_lines.find('G') != lookup_lines.end() && lookup_lines['G'].empty())
		return NULL;

	for (std::vector<XLine*>::iterator i = active_lines.begin(); i != active_lines.end(); i++)
		if ((*i)->type == 'G' && (*i)->Matches(user))
			return (GLine*)(*i);

	return NULL;
}

ELine* XLineManager::matches_exception(User* user)
{
	if (lookup_lines.find('E') == lookup_lines.end())
		return NULL;

	if (lookup_lines.find('E') != lookup_lines.end() && lookup_lines['E'].empty())
		return NULL;

	for (std::vector<XLine*>::iterator i = active_lines.begin(); i != active_lines.end(); i++)
	{
		if ((*i)->type == 'E' && (*i)->Matches(user))
			return (ELine*)(*i);
	}
	return NULL;
}


void XLineManager::gline_set_creation_time(const char* host, time_t create_time)
{
	/*for (std::vector<XLine*>::iterator i = glines.begin(); i != glines.end(); i++)
	{
		if (!strcasecmp(host,(*i)->hostmask))
		{
			(*i)->set_time = create_time;
			(*i)->expiry = create_time + (*i)->duration;
			return;
		}
	}*/

	return ;
}

void XLineManager::eline_set_creation_time(const char* host, time_t create_time)
{
	/*for (std::vector<ELine*>::iterator i = elines.begin(); i != elines.end(); i++)
	{
		if (!strcasecmp(host,(*i)->hostmask))
		{
			(*i)->set_time = create_time;
			(*i)->expiry = create_time + (*i)->duration;
			return;
		}
	}*/

	return;
}

void XLineManager::qline_set_creation_time(const char* nick, time_t create_time)
{
	/*for (std::vector<QLine*>::iterator i = qlines.begin(); i != qlines.end(); i++)
	{
		if (!strcasecmp(nick,(*i)->nick))
		{
			(*i)->set_time = create_time;
			(*i)->expiry = create_time + (*i)->duration;
			return;
		}
	}*/

	return;
}

void XLineManager::zline_set_creation_time(const char* ip, time_t create_time)
{
	/*for (std::vector<ZLine*>::iterator i = zlines.begin(); i != zlines.end(); i++)
	{
		if (!strcasecmp(ip,(*i)->ipaddr))
		{
			(*i)->set_time = create_time;
			(*i)->expiry = create_time + (*i)->duration;
			return;
		}
	}*/

	return;
}

// returns a pointer to the reason if an ip address matches a zline, NULL if it didnt match

ZLine* XLineManager::matches_zline(User *u)
{
	if (lookup_lines.find('Z') == lookup_lines.end())
		return NULL;

	if (lookup_lines.find('Z') != lookup_lines.end() && lookup_lines['Z'].empty())
		return NULL;

	for (std::vector<XLine*>::iterator i = active_lines.begin(); i != active_lines.end(); i++)
		if ((*i)->type == 'Z' && (*i)->Matches(u))
			return (ZLine*)(*i);
	return NULL;
}

// returns a pointer to the reason if a host matches a kline, NULL if it didnt match

KLine* XLineManager::matches_kline(User* user)
{
	if (lookup_lines.find('K') == lookup_lines.end())
		return NULL;

	if (lookup_lines.find('K') != lookup_lines.end() && lookup_lines['K'].empty())
		return NULL;

	for (std::vector<XLine*>::iterator i = active_lines.begin(); i != active_lines.end(); i++)
		if ((*i)->Matches(user))
			return (KLine*)(*i);

	return NULL;
}

bool XLineManager::XSortComparison(const XLine *one, const XLine *two)
{
	return (one->expiry) < (two->expiry);
}

// removes lines that have expired
void XLineManager::expire_lines()
{
	time_t current = ServerInstance->Time();

	/* Because we now store all our XLines in sorted order using ((*i)->duration + (*i)->set_time) as a key, this
	 * means that to expire the XLines we just need to do a while, picking off the top few until there are
	 * none left at the head of the queue that are after the current time. We use PermLines as an offset into the
	 * vector past the first item with a duration 0.
	 */

	std::vector<XLine*>::iterator start = active_lines.begin() + PermLines;

	while ((start < active_lines.end()) && (current > (*start)->expiry))
	{
		(*start)->DisplayExpiry();
		(*start)->Unset();

		if (lookup_lines.find((*start)->type) != lookup_lines.end())
			lookup_lines[(*start)->type].erase((*start)->Displayable());

		std::vector<XLine*>::iterator pptr = std::find(pending_lines.begin(), pending_lines.end(), *start);
		if (pptr != pending_lines.end())
			pending_lines.erase(pptr);

		if (!(*start)->duration)
			PermLines--;

		delete *start;
		active_lines.erase(start);
	}
}

// applies lines, removing clients and changing nicks etc as applicable
void XLineManager::ApplyLines()
{
	for (std::vector<User*>::const_iterator u2 = ServerInstance->local_users.begin(); u2 != ServerInstance->local_users.end(); u2++)
	{
		User* u = (User*)(*u2);

		for (std::vector<XLine *>::iterator i = pending_lines.begin(); i != pending_lines.end(); i++)
		{
			XLine *x = *i;
			if (x->Matches(u))
				x->Apply(u);
		}
	}

	pending_lines.clear();
}

/* k: 216
 * g: 223
 * q: 217
 * z: 223
 * e: 223
 */

void XLineManager::InvokeStats(const char type, int numeric, User* user, string_list &results)
{
	std::string sn = ServerInstance->Config->ServerName;

	std::map<const char, std::map<std::string, XLine*> >::iterator n = lookup_lines.find(type);

	if (n != lookup_lines.end())
	{
		std::map<std::string, XLine*>& list = n->second;
		for (std::map<std::string, XLine*>::iterator i = list.begin(); i != list.end(); i++)
			results.push_back(sn+" "+ConvToStr(numeric)+" "+user->nick+" :"+i->second->Displayable()+" "+
					ConvToStr(i->second->set_time)+" "+ConvToStr(i->second->duration)+" "+std::string(i->second->source)+" :"+(i->second->reason));
	}
}


XLineManager::XLineManager(InspIRCd* Instance) : ServerInstance(Instance), PermLines(0)
{
	GFact = new GLineFactory(Instance);
	EFact = new ELineFactory(Instance);
	KFact = new KLineFactory(Instance);
	QFact = new QLineFactory(Instance);
	ZFact = new ZLineFactory(Instance);

	RegisterFactory(GFact);
	RegisterFactory(EFact);
	RegisterFactory(KFact);
	RegisterFactory(QFact);
	RegisterFactory(ZFact);
}

XLineManager::~XLineManager()
{
	UnregisterFactory(GFact);
	UnregisterFactory(EFact);
	UnregisterFactory(KFact);
	UnregisterFactory(QFact);
	UnregisterFactory(ZFact);

	delete GFact;
	delete EFact;
	delete KFact;
	delete QFact;
	delete ZFact;
}

void XLine::Apply(User* u)
{
}

void XLine::DefaultApply(User* u, char line)
{
	char reason[MAXBUF];
	snprintf(reason, MAXBUF, "%c-Lined: %s", line, this->reason);
	if (*ServerInstance->Config->MoronBanner)
		u->WriteServ("NOTICE %s :*** %s", u->nick, ServerInstance->Config->MoronBanner);
	if (ServerInstance->Config->HideBans)
		User::QuitUser(ServerInstance, u, line + std::string("-Lined"), reason);
	else
		User::QuitUser(ServerInstance, u, reason);
}

bool KLine::Matches(User *u)
{
	if (u->exempt)
		return false;

	if ((match(u->ident, this->identmask)))
	{
		if ((match(u->host, this->hostmask, true)) || (match(u->GetIPString(), this->hostmask, true)))
		{
			return true;
		}
	}

	return false;
}

void KLine::Apply(User* u)
{
	DefaultApply(u, 'K');
}

bool GLine::Matches(User *u)
{
	if (u->exempt)
		return false;

	if ((match(u->ident, this->identmask)))
	{
		if ((match(u->host, this->hostmask, true)) || (match(u->GetIPString(), this->hostmask, true)))
		{
			return true;
		}
	}

	return false;
}

void GLine::Apply(User* u)
{       
	DefaultApply(u, 'G');
}

bool ELine::Matches(User *u)
{
	if (u->exempt)
		return false;

	if ((match(u->ident, this->identmask)))
	{
		if ((match(u->host, this->hostmask, true)) || (match(u->GetIPString(), this->hostmask, true)))
		{
			return true;
		}
	}

	return false;
}

bool ZLine::Matches(User *u)
{
	if (u->exempt)
		return false;

	if (match(u->GetIPString(), this->ipaddr, true))
		return true;
	else
		return false;
}

void ZLine::Apply(User* u)
{       
	DefaultApply(u, 'Z');
}


bool QLine::Matches(User *u)
{
	if (u->exempt)
		return false;

	if (match(u->nick, this->nick))
		return true;

	return false;
}

void QLine::Apply(User* u)
{       
	/* Can we force the user to their uid here instead? */
	DefaultApply(u, 'Q');
}


bool ZLine::Matches(const std::string &str)
{
	if (match(str.c_str(), this->ipaddr, true))
		return true;
	else
		return false;
}

bool QLine::Matches(const std::string &str)
{
	if (match(str.c_str(), this->nick))
		return true;

	return false;
}

bool ELine::Matches(const std::string &str)
{
	return ((match(str.c_str(), matchtext.c_str(), true)));
}

bool KLine::Matches(const std::string &str)
{
	return ((match(str.c_str(), matchtext.c_str(), true)));
}

bool GLine::Matches(const std::string &str)
{
	return ((match(str.c_str(), matchtext.c_str(), true)));
}

bool ELine::MatchesLiteral(const std::string &str)
{
	return (assign(str) == matchtext);
}

bool ZLine::MatchesLiteral(const std::string &str)
{       
	return (assign(str) == this->ipaddr);
}

bool GLine::MatchesLiteral(const std::string &str)
{       
	return (assign(str) == matchtext);
}

bool KLine::MatchesLiteral(const std::string &str)
{       
	return (assign(str) == matchtext);
}

bool QLine::MatchesLiteral(const std::string &str)
{       
	return (assign(str) == this->nick);
}

void ELine::OnAdd()
{
	/* When adding one eline, only check the one eline */
	for (std::vector<User*>::const_iterator u2 = ServerInstance->local_users.begin(); u2 != ServerInstance->local_users.end(); u2++)
	{
		User* u = (User*)(*u2);
		if (this->Matches(u))
			u->exempt = true;
	}
}

void ELine::DisplayExpiry()
{
	ServerInstance->SNO->WriteToSnoMask('x',"Expiring timed E-Line %s@%s (set by %s %d seconds ago)",this->identmask,this->hostmask,this->source,this->duration);
}

void QLine::DisplayExpiry()
{
	ServerInstance->SNO->WriteToSnoMask('x',"Expiring timed Q-Line %s (set by %s %d seconds ago)",this->nick,this->source,this->duration);
}

void ZLine::DisplayExpiry()
{
	ServerInstance->SNO->WriteToSnoMask('x',"Expiring timed Z-Line %s (set by %s %d seconds ago)",this->ipaddr,this->source,this->duration);
}

void KLine::DisplayExpiry()
{
	ServerInstance->SNO->WriteToSnoMask('x',"Expiring timed K-Line %s@%s (set by %s %d seconds ago)",this->identmask,this->hostmask,this->source,this->duration);
}

void GLine::DisplayExpiry()
{
	ServerInstance->SNO->WriteToSnoMask('x',"Expiring timed G-Line %s@%s (set by %s %d seconds ago)",this->identmask,this->hostmask,this->source,this->duration);
}

const char* ELine::Displayable()
{
	return matchtext.c_str();
}

const char* KLine::Displayable()
{
	return matchtext.c_str();
}

const char* GLine::Displayable()
{
	return matchtext.c_str();
}

const char* ZLine::Displayable()
{
	return ipaddr;
}

const char* QLine::Displayable()
{
	return nick;
}

bool XLineManager::RegisterFactory(XLineFactory* xlf)
{
	std::map<char, XLineFactory*>::iterator n = line_factory.find(xlf->GetType());

	if (n != line_factory.end())
		return false;

	line_factory[xlf->GetType()] = xlf;

	return true;
}

bool XLineManager::UnregisterFactory(XLineFactory* xlf)
{
	std::map<char, XLineFactory*>::iterator n = line_factory.find(xlf->GetType());

	if (n == line_factory.end())
		return false;

	line_factory.erase(n);

	return true;
}

XLineFactory* XLineManager::GetFactory(const char type)
{
	std::map<char, XLineFactory*>::iterator n = line_factory.find(type);

	if (n != line_factory.end())
		return NULL;

	return n->second;
}

