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
#include "bancache.h"

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
 *  All lines are (as in v1) stored together -- no seperation of perm and non-perm. They are stored in
 *  a map of maps (first map is line type, second map is for quick lookup on add/delete/etc).
 *
 *  Expiry is *no longer* performed on a timer, and no longer uses a sorted list of any variety. This
 *  is now done by only checking for expiry when a line is accessed, meaning that expiry is no longer
 *  a resource intensive problem.
 *
 *  Application no longer tries to apply every single line on every single user - instead, now only lines
 *  added since the previous application are applied. This keeps S2S ADDLINE during burst nice and fast,
 *  while at the same time not slowing things the fuck down when we try adding a ban with lots of preexisting
 *  bans. :)
 */

bool XLine::Matches(User *u)
{
	return false;
}

/*
 * Checks what users match a given vector of ELines and sets their ban exempt flag accordingly.
 */
void XLineManager::CheckELines()
{
	ContainerIter n = lookup_lines.find("E");

	if (n == lookup_lines.end())
		return;

	XLineLookup& ELines = n->second;

	if (ELines.empty())
		return;

	for (std::vector<User*>::const_iterator u2 = ServerInstance->local_users.begin(); u2 != ServerInstance->local_users.end(); u2++)
	{
		User* u = (User*)(*u2);

		/* This uses safe iteration to ensure that if a line expires here, it doenst trash the iterator */
		LookupIter safei;

		for (LookupIter i = ELines.begin(); i != ELines.end(); )
		{
			safei = i;
			safei++;

			XLine *e = i->second;
			u->exempt = e->Matches(u);

			i = safei;
		}
	}
}


XLineLookup* XLineManager::GetAll(const std::string &type)
{
	ContainerIter n = lookup_lines.find(type);

	if (n == lookup_lines.end())
		return NULL;

	LookupIter safei;
	const time_t current = ServerInstance->Time();

	/* Expire any dead ones, before sending */
	for (LookupIter x = n->second.begin(); x != n->second.end(); )
	{
		safei = x;
		safei++;
		if (x->second->duration && current > x->second->expiry)
		{
			ExpireLine(n, x);
		}
		x = safei;
	}

	return &(n->second);
}

std::vector<std::string> XLineManager::GetAllTypes()
{
	std::vector<std::string> items;
	for (ContainerIter x = lookup_lines.begin(); x != lookup_lines.end(); ++x)
		items.push_back(x->first);
	return items;
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

// adds a line

bool XLineManager::AddLine(XLine* line, User* user)
{
	/*IdentHostPair ih = IdentSplit(hostmask);*/

	if (DelLine(line->Displayable(), line->type, user, true))
		return false;

	/*ELine* item = new ELine(ServerInstance, ServerInstance->Time(), duration, source, reason, ih.first.c_str(), ih.second.c_str());*/
	pending_lines.push_back(line);
	lookup_lines[line->type][line->Displayable()] = line;
	line->OnAdd();

	FOREACH_MOD(I_OnAddLine,OnAddLine(user, line));	

	return true;
}

// deletes a line, returns true if the line existed and was removed

bool XLineManager::DelLine(const char* hostmask, const std::string &type, User* user, bool simulate)
{
	ContainerIter x = lookup_lines.find(type);

	if (x == lookup_lines.end())
		return false;

	LookupIter y = x->second.find(hostmask);

	if (y == x->second.end())
		return false;

	if (simulate)
		return true;

	FOREACH_MOD(I_OnDelLine,OnDelLine(user, y->second));

	y->second->Unset();

	std::vector<XLine*>::iterator pptr = std::find(pending_lines.begin(), pending_lines.end(), y->second);
	if (pptr != pending_lines.end())
		pending_lines.erase(pptr);

	delete y->second;
	x->second.erase(y);

	return true;
}


void ELine::Unset()
{
	/* remove exempt from everyone and force recheck after deleting eline */
	for (std::vector<User*>::const_iterator u2 = ServerInstance->local_users.begin(); u2 != ServerInstance->local_users.end(); u2++)
	{
		User* u = (User*)(*u2);
		u->exempt = false;
	}

	ServerInstance->XLines->CheckELines();
}

// returns a pointer to the reason if a nickname matches a qline, NULL if it didnt match

XLine* XLineManager::MatchesLine(const std::string &type, User* user)
{
	ContainerIter x = lookup_lines.find(type);

	if (x == lookup_lines.end())
		return NULL;

	const time_t current = ServerInstance->Time();

	LookupIter safei;

	for (LookupIter i = x->second.begin(); i != x->second.end(); )
	{
		safei = i;
		safei++;

		if (i->second->Matches(user))
		{
			if (i->second->duration && current > i->second->expiry)
			{
				/* Expire the line, return nothing */
				ExpireLine(x, i);
				/* Continue, there may be another that matches
				 * (thanks aquanight)
				 */
				i = safei;
				continue;
			}
			else
				return i->second;
		}

		i = safei;
	}
	return NULL;
}

XLine* XLineManager::MatchesLine(const std::string &type, const std::string &pattern)
{
	ContainerIter x = lookup_lines.find(type);

	if (x == lookup_lines.end())
		return NULL;

	const time_t current = ServerInstance->Time();

	 LookupIter safei;

	for (LookupIter i = x->second.begin(); i != x->second.end(); )
	{
		safei = i;
		safei++;

		if (i->second->Matches(pattern))
		{
			if (i->second->duration && current > i->second->expiry)
			{
				/* Expire the line, return nothing */
				ExpireLine(x, i);
				/* See above */
				i = safei;
				continue;
			}
			else
				return i->second;
		}

		i = safei;
	}
	return NULL;
}

// removes lines that have expired
void XLineManager::ExpireLine(ContainerIter container, LookupIter item)
{
		item->second->DisplayExpiry();
		item->second->Unset();

		/* TODO: Can we skip this loop by having a 'pending' field in the XLine class, which is set when a line
		 * is pending, cleared when it is no longer pending, so we skip over this loop if its not pending?
		 * -- Brain
		 */
		std::vector<XLine*>::iterator pptr = std::find(pending_lines.begin(), pending_lines.end(), item->second);
		if (pptr != pending_lines.end())
			pending_lines.erase(pptr);

		delete item->second;
		container->second.erase(item);
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

void XLineManager::InvokeStats(const std::string &type, int numeric, User* user, string_list &results)
{
	std::string sn = ServerInstance->Config->ServerName;

	ContainerIter n = lookup_lines.find(type);

	time_t current = ServerInstance->Time();

	LookupIter safei;

	if (n != lookup_lines.end())
	{
		XLineLookup& list = n->second;
		for (LookupIter i = list.begin(); i != list.end(); )
		{
			safei = i;
			safei++;

			if (i->second->duration && current > i->second->expiry)
			{
				ExpireLine(n, i);
			}
			else
				results.push_back(sn+" "+ConvToStr(numeric)+" "+user->nick+" :"+i->second->Displayable()+" "+
					ConvToStr(i->second->set_time)+" "+ConvToStr(i->second->duration)+" "+std::string(i->second->source)+" :"+(i->second->reason));
			i = safei;
		}
	}
}


XLineManager::XLineManager(InspIRCd* Instance) : ServerInstance(Instance)
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

void XLine::DefaultApply(User* u, const std::string &line, bool bancache)
{
	char reason[MAXBUF];
	snprintf(reason, MAXBUF, "%s-Lined: %s", line.c_str(), this->reason);
	if (*ServerInstance->Config->MoronBanner)
		u->WriteServ("NOTICE %s :*** %s", u->nick, ServerInstance->Config->MoronBanner);
	if (ServerInstance->Config->HideBans)
		User::QuitUser(ServerInstance, u, line + "-Lined", reason);
	else
		User::QuitUser(ServerInstance, u, reason);


	if (bancache)
	{
		ServerInstance->Log(DEBUG, std::string("BanCache: Adding positive hit (") + line + ") for " + u->GetIPString());
		ServerInstance->BanCache->AddHit(u->GetIPString(), this->type, line + "-Lined: " + this->reason);
	}
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
	DefaultApply(u, "K", (strcmp(this->identmask, "*") == 0) ? true : false);
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
	DefaultApply(u, "G", (strcmp(this->identmask, "*") == 0) ? true : false);
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
	DefaultApply(u, "Z", true);
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
	/* Force to uuid on apply of qline, no need to disconnect any more :) */
	u->ForceNickChange(u->uuid);
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
	XLineFactMap::iterator n = line_factory.find(xlf->GetType());

	if (n != line_factory.end())
		return false;

	line_factory[xlf->GetType()] = xlf;

	return true;
}

bool XLineManager::UnregisterFactory(XLineFactory* xlf)
{
	XLineFactMap::iterator n = line_factory.find(xlf->GetType());

	if (n == line_factory.end())
		return false;

	line_factory.erase(n);

	return true;
}

XLineFactory* XLineManager::GetFactory(const std::string &type)
{
	XLineFactMap::iterator n = line_factory.find(type);

	if (n != line_factory.end())
		return NULL;

	return n->second;
}

