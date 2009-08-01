/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */

#include "inspircd.h"
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

	for (std::vector<User*>::const_iterator u2 = ServerInstance->Users->local_users.begin(); u2 != ServerInstance->Users->local_users.end(); u2++)
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

void XLineManager::DelAll(const std::string &type)
{
	ContainerIter n = lookup_lines.find(type);

	if (n == lookup_lines.end())
		return;

	LookupIter x;

	/* Delete all of a given type (this should probably use DelLine, but oh well) */
	while ((x = n->second.begin()) != n->second.end())
	{
		ExpireLine(n, x);
	}
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
		n.first = "";
		n.second = ident_and_host;
	}

	return n;
}

// adds a line

bool XLineManager::AddLine(XLine* line, User* user)
{
	ServerInstance->BanCache->RemoveEntries(line->type, false); // XXX perhaps remove ELines here?

	if (line->duration && ServerInstance->Time() > line->expiry)
		return false; // Don't apply expired XLines.

	/* Don't apply duplicate xlines */
	ContainerIter x = lookup_lines.find(line->type);
	if (x != lookup_lines.end())
	{
		LookupIter i = x->second.find(line->Displayable());
		if (i != x->second.end())
		{
			return false;
		}
	}

	/*ELine* item = new ELine(ServerInstance, ServerInstance->Time(), duration, source, reason, ih.first.c_str(), ih.second.c_str());*/
	XLineFactory* xlf = GetFactory(line->type);
	if (!xlf)
		return false;

	if (xlf->AutoApplyToUserList(line))
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

	ServerInstance->BanCache->RemoveEntries(y->second->type, true);

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
	for (std::vector<User*>::const_iterator u2 = ServerInstance->Users->local_users.begin(); u2 != ServerInstance->Users->local_users.end(); u2++)
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

		if (i->second->duration && current > i->second->expiry)
		{
			/* Expire the line, proceed to next one */
			ExpireLine(x, i);
			i = safei;
			continue;
		}

		if (i->second->Matches(user))
		{
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
	FOREACH_MOD(I_OnExpireLine, OnExpireLine(item->second));

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
	for (std::vector<User*>::const_iterator u2 = ServerInstance->Users->local_users.begin(); u2 != ServerInstance->Users->local_users.end(); u2++)
	{
		User* u = (User*)(*u2);

		// Don't ban people who are exempt.
		if (u->exempt)
			continue;

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

	// Delete all existing XLines
	for (XLineContainer::iterator i = lookup_lines.begin(); i != lookup_lines.end(); i++)
	{
		for (XLineLookup::iterator j = i->second.begin(); j != i->second.end(); j++)
		{
			delete j->second;
		}
		i->second.clear();
	}
	lookup_lines.clear();

}

void XLine::Apply(User* u)
{
}

bool XLine::IsBurstable()
{
	return true;
}

void XLine::DefaultApply(User* u, const std::string &line, bool bancache)
{
	char sreason[MAXBUF];
	snprintf(sreason, MAXBUF, "%s-Lined: %s", line.c_str(), this->reason);
	if (*ServerInstance->Config->MoronBanner)
		u->WriteServ("NOTICE %s :*** %s", u->nick.c_str(), ServerInstance->Config->MoronBanner);

	if (ServerInstance->Config->HideBans)
		ServerInstance->Users->QuitUser(u, line + "-Lined", sreason);
	else
		ServerInstance->Users->QuitUser(u, sreason);


	if (bancache)
	{
		ServerInstance->Logs->Log("BANCACHE", DEBUG, std::string("BanCache: Adding positive hit (") + line + ") for " + u->GetIPString());
		if (this->duration > 0)
			ServerInstance->BanCache->AddHit(u->GetIPString(), this->type, line + "-Lined: " + this->reason, this->duration);
		else
			ServerInstance->BanCache->AddHit(u->GetIPString(), this->type, line + "-Lined: " + this->reason);
	}
}

bool KLine::Matches(User *u)
{
	if (u->exempt)
		return false;

	if (InspIRCd::Match(u->ident, this->identmask, ascii_case_insensitive_map))
	{
		if (InspIRCd::MatchCIDR(u->host, this->hostmask, ascii_case_insensitive_map) ||
		    InspIRCd::MatchCIDR(u->GetIPString(), this->hostmask, ascii_case_insensitive_map))
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

	if (InspIRCd::Match(u->ident, this->identmask, ascii_case_insensitive_map))
	{
		if (InspIRCd::MatchCIDR(u->host, this->hostmask, ascii_case_insensitive_map) ||
		    InspIRCd::MatchCIDR(u->GetIPString(), this->hostmask, ascii_case_insensitive_map))
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

	if (InspIRCd::Match(u->ident, this->identmask, ascii_case_insensitive_map))
	{
		if (InspIRCd::MatchCIDR(u->host, this->hostmask, ascii_case_insensitive_map) ||
		    InspIRCd::MatchCIDR(u->GetIPString(), this->hostmask, ascii_case_insensitive_map))
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

	if (InspIRCd::MatchCIDR(u->GetIPString(), this->ipaddr))
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
	if (InspIRCd::Match(u->nick, this->nick))
		return true;

	return false;
}

void QLine::Apply(User* u)
{
	/* Force to uuid on apply of qline, no need to disconnect any more :) */
	u->ForceNickChange(u->uuid.c_str());
}


bool ZLine::Matches(const std::string &str)
{
	if (InspIRCd::MatchCIDR(str, this->ipaddr))
		return true;
	else
		return false;
}

bool QLine::Matches(const std::string &str)
{
	if (InspIRCd::Match(str, this->nick))
		return true;

	return false;
}

bool ELine::Matches(const std::string &str)
{
	return (InspIRCd::MatchCIDR(str, matchtext));
}

bool KLine::Matches(const std::string &str)
{
	return (InspIRCd::MatchCIDR(str.c_str(), matchtext));
}

bool GLine::Matches(const std::string &str)
{
	return (InspIRCd::MatchCIDR(str, matchtext));
}

void ELine::OnAdd()
{
	/* When adding one eline, only check the one eline */
	for (std::vector<User*>::const_iterator u2 = ServerInstance->Users->local_users.begin(); u2 != ServerInstance->Users->local_users.end(); u2++)
	{
		User* u = (User*)(*u2);
		if (this->Matches(u))
			u->exempt = true;
	}
}

void ELine::DisplayExpiry()
{
	ServerInstance->SNO->WriteToSnoMask('x',"Removing expired E-Line %s@%s (set by %s %ld seconds ago)",this->identmask,this->hostmask,this->source,(long int)(ServerInstance->Time() - this->set_time));
}

void QLine::DisplayExpiry()
{
	ServerInstance->SNO->WriteToSnoMask('x',"Removing expired Q-Line %s (set by %s %ld seconds ago)",this->nick,this->source,(long int)(ServerInstance->Time() - this->set_time));
}

void ZLine::DisplayExpiry()
{
	ServerInstance->SNO->WriteToSnoMask('x',"Removing expired Z-Line %s (set by %s %ld seconds ago)",this->ipaddr,this->source,(long int)(ServerInstance->Time() - this->set_time));
}

void KLine::DisplayExpiry()
{
	ServerInstance->SNO->WriteToSnoMask('x',"Removing expired K-Line %s@%s (set by %s %ld seconds ago)",this->identmask,this->hostmask,this->source,(long int)(ServerInstance->Time() - this->set_time));
}

void GLine::DisplayExpiry()
{
	ServerInstance->SNO->WriteToSnoMask('x',"Removing expired G-Line %s@%s (set by %s %ld seconds ago)",this->identmask,this->hostmask,this->source,(long int)(ServerInstance->Time() - this->set_time));
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

bool KLine::IsBurstable()
{
	return false;
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

	if (n == line_factory.end())
		return NULL;

	return n->second;
}
