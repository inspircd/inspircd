/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2017-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004, 2006-2008 Craig Edwards <brain@inspircd.org>
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
#include "modules/stats.h"
#include "timeutils.h"
#include "xline.h"

/** An XLineFactory specialized to generate GLine* pointers
 */
class GLineFactory final
	: public XLineFactory
{
public:
	GLineFactory()
		: XLineFactory("G")
	{
	}

	/** Generate a GLine
	 */
	XLine* Generate(time_t set_time, unsigned long duration, const std::string& source, const std::string& reason, const std::string& xline_specific_mask) override
	{
		IdentHostPair ih = XLineManager::IdentSplit(xline_specific_mask);
		return new GLine(set_time, duration, source, reason, ih.first, ih.second);
	}
};

/** An XLineFactory specialized to generate ELine* pointers
 */
class ELineFactory final
	: public XLineFactory
{
public:
	ELineFactory()
		: XLineFactory("E")
	{
	}

	/** Generate an ELine
	 */
	XLine* Generate(time_t set_time, unsigned long duration, const std::string& source, const std::string& reason, const std::string& xline_specific_mask) override
	{
		IdentHostPair ih = XLineManager::IdentSplit(xline_specific_mask);
		return new ELine(set_time, duration, source, reason, ih.first, ih.second);
	}
};

/** An XLineFactory specialized to generate KLine* pointers
 */
class KLineFactory final
	: public XLineFactory
{
public:
	KLineFactory()
		: XLineFactory("K")
	{
	}

	/** Generate a KLine
	 */
	XLine* Generate(time_t set_time, unsigned long duration, const std::string& source, const std::string& reason, const std::string& xline_specific_mask) override
	{
		IdentHostPair ih = XLineManager::IdentSplit(xline_specific_mask);
		return new KLine(set_time, duration, source, reason, ih.first, ih.second);
	}
};

/** An XLineFactory specialized to generate QLine* pointers
 */
class QLineFactory final
	: public XLineFactory
{
public:
	QLineFactory()
		: XLineFactory("Q")
	{
	}

	/** Generate a QLine
	 */
	XLine* Generate(time_t set_time, unsigned long duration, const std::string& source, const std::string& reason, const std::string& xline_specific_mask) override
	{
		return new QLine(set_time, duration, source, reason, xline_specific_mask);
	}
};

/** An XLineFactory specialized to generate ZLine* pointers
 */
class ZLineFactory final
	: public XLineFactory
{
public:
	ZLineFactory()
		: XLineFactory("Z")
	{
	}

	/** Generate a ZLine
	 */
	XLine* Generate(time_t set_time, unsigned long duration, const std::string& source, const std::string& reason, const std::string& xline_specific_mask) override
	{
		return new ZLine(set_time, duration, source, reason, xline_specific_mask);
	}
};

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
 *  Version 2 moved permanent lines into a separate list from non-permanent to help optimize
 *  matching speed, but matched in the same way.
 *  Expiry was also sped up by sorting the list by expiry (meaning just remove the items at the
 *  head of the list that are outdated.)
 *
 * This was fine and good, but it looked less than ideal in code, and matching was still slower
 * than it could have been, something which we address here.
 *
 * VERSION 3:
 *  All lines are (as in v1) stored together -- no separation of perm and non-perm. They are stored in
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

bool XLine::Matches(User* u) const
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

	for (auto* u :  ServerInstance->Users.GetLocalUsers())
	{
		u->exempt = false;

		/* This uses safe iteration to ensure that if a line expires here, it doesn't trash the iterator */
		LookupIter safei;

		for (LookupIter i = ELines.begin(); i != ELines.end(); )
		{
			safei = i;
			safei++;

			XLine* e = i->second;
			if ((!e->duration || ServerInstance->Time() < e->expiry) && e->Matches(u))
				u->exempt = true;

			i = safei;
		}
	}
}

XLineLookup* XLineManager::GetAll(const std::string& type)
{
	ContainerIter n = lookup_lines.find(type);

	if (n == lookup_lines.end())
		return nullptr;

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

void XLineManager::DelAll(const std::string& type)
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
	for (const auto& [type, _] : lookup_lines)
		items.push_back(type);
	return items;
}

IdentHostPair XLineManager::IdentSplit(const std::string& ident_and_host)
{
	IdentHostPair n = std::make_pair<std::string, std::string>("*", "*");
	std::string::size_type x = ident_and_host.find('@');
	if (x != std::string::npos)
	{
		n.second = ident_and_host.substr(x + 1, ident_and_host.length());
		n.first = ident_and_host.substr(0, x);
		if (!n.first.length())
			n.first.assign("*");
		if (!n.second.length())
			n.second.assign("*");
	}
	else
	{
		n.first.clear();
		n.second = ident_and_host;
	}

	return n;
}

// adds a line

bool XLineManager::AddLine(XLine* line, User* user)
{
	if (line->duration && ServerInstance->Time() > line->expiry)
		return false; // Don't apply expired XLines.

	/* Don't apply duplicate xlines */
	ContainerIter x = lookup_lines.find(line->type);
	if (x != lookup_lines.end())
	{
		LookupIter i = x->second.find(line->Displayable());
		if (i != x->second.end())
		{
			bool silent = false;

			// Allow replacing a config line for an updated config line.
			if (i->second->from_config && line->from_config)
			{
				// Nothing changed, skip adding this one.
				if (i->second->reason == line->reason)
					return false;

				silent = true;
			}
			// Allow replacing a non-config line for a new config line.
			else if (!line->from_config)
			{
				// X-line propagation bug was here, if the line to be added already exists and
				// it's expired then expire it and add the new one instead of returning false
				if ((!i->second->duration) || (ServerInstance->Time() < i->second->expiry))
					return false;
			}
			else
			{
				silent = true;
			}

			ExpireLine(x, i, silent);
		}
	}

	XLineFactory* xlf = GetFactory(line->type);
	if (!xlf)
		return false;

	ServerInstance->BanCache.RemoveEntries(line->type, false); // XXX perhaps remove ELines here?

	if (xlf->AutoApplyToUserList(line))
		pending_lines.push_back(line);

	lookup_lines[line->type][line->Displayable()] = line;
	line->OnAdd();

	FOREACH_MOD(OnAddLine, (user, line));

	return true;
}

// deletes a line, returns true if the line existed and was removed

bool XLineManager::DelLine(const std::string& hostmask, const std::string& type, std::string& reason, User* user, bool simulate)
{
	ContainerIter x = lookup_lines.find(type);

	if (x == lookup_lines.end())
		return false;

	LookupIter y = x->second.find(hostmask);

	if (y == x->second.end())
		return false;

	reason.assign(y->second->reason);

	if (simulate)
		return true;

	ServerInstance->BanCache.RemoveEntries(y->second->type, true);

	FOREACH_MOD(OnDelLine, (user, y->second));

	y->second->Unset();

	stdalgo::erase(pending_lines, y->second);

	delete y->second;
	x->second.erase(y);

	return true;
}

void ELine::Unset()
{
	ServerInstance->XLines->CheckELines();
}

// returns a pointer to the reason if a nickname matches a Q-line, NULL if it didn't match

XLine* XLineManager::MatchesLine(const std::string& type, User* user)
{
	ContainerIter x = lookup_lines.find(type);

	if (x == lookup_lines.end())
		return nullptr;

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
	return nullptr;
}

XLine* XLineManager::MatchesLine(const std::string& type, const std::string& pattern)
{
	ContainerIter x = lookup_lines.find(type);

	if (x == lookup_lines.end())
		return nullptr;

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
	return nullptr;
}

// removes lines that have expired
void XLineManager::ExpireLine(ContainerIter container, LookupIter item, bool silent)
{
	ServerInstance->BanCache.RemoveEntries(item->second->type, true);
	FOREACH_MOD(OnExpireLine, (item->second));

	if (!silent)
		item->second->DisplayExpiry();

	item->second->Unset();

	/* TODO: Can we skip this loop by having a 'pending' field in the XLine class, which is set when a line
	 * is pending, cleared when it is no longer pending, so we skip over this loop if its not pending?
	 * -- Brain
	 */
	stdalgo::erase(pending_lines, item->second);

	delete item->second;
	container->second.erase(item);
}

// applies lines, removing clients and changing nicks etc as applicable
void XLineManager::ApplyLines()
{
	const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
	for (UserManager::LocalList::const_iterator j = list.begin(); j != list.end(); )
	{
		LocalUser* u = *j++;

		// Don't ban people who are exempt.
		if (u->exempt)
			continue;

		for (auto* x : pending_lines)
		{
			if (x->Matches(u))
			{
				x->Apply(u);

				// If applying the X-line has killed the user then don't
				// apply any more lines to them.
				if (u->quitting)
					break;
			}
		}
	}

	pending_lines.clear();
}

bool XLineManager::InvokeStats(const std::string& type, Stats::Context& context)
{
	ContainerIter citer = lookup_lines.find(type);
	if (citer == lookup_lines.end())
		return false;

	for (LookupIter liter = citer->second.begin(); liter != citer->second.end(); )
	{
		// We might be about to expire the XLine so we have to increment the
		// iterator early to avoid doing that causing iterator invalidation.
		LookupIter current = liter++;

		XLine* xline = current->second;
		if (xline->duration && xline->expiry <= ServerInstance->Time())
		{
			// This XLine has expired so remove and skip it.
			ExpireLine(citer, current);
			continue;
		}

		context.AddGenericRow(xline->Displayable(), xline->set_time, xline->duration, xline->source, xline->reason);
	}
	return true;
}

XLineManager::XLineManager()
{
	GLineFactory* GFact;
	ELineFactory* EFact;
	KLineFactory* KFact;
	QLineFactory* QFact;
	ZLineFactory* ZFact;

	GFact = new GLineFactory;
	EFact = new ELineFactory;
	KFact = new KLineFactory;
	QFact = new QLineFactory;
	ZFact = new ZLineFactory;

	RegisterFactory(GFact);
	RegisterFactory(EFact);
	RegisterFactory(KFact);
	RegisterFactory(QFact);
	RegisterFactory(ZFact);
}

XLineManager::~XLineManager()
{
	for(const char line : "GEKQZ")
	{
		XLineFactory* xlf = GetFactory(std::string(1, line));
		delete xlf;
	}

	// Delete all existing XLines
	for (const auto& [_, lines] : lookup_lines)
	{
		for (const auto& [__, line] : lines)
			delete line;
	}
}

void XLine::Apply(User* u)
{
}

bool XLine::IsBurstable()
{
	return !from_config;
}

void XLine::DefaultApply(User* u, const std::string& line, bool bancache)
{
	if (!ServerInstance->Config->XLineMessage.empty())
		u->WriteNumeric(ERR_YOUREBANNEDCREEP, ServerInstance->Config->XLineMessage);

	const std::string banreason = line + "-lined: " + reason;
	if (ServerInstance->Config->HideLines.empty())
		ServerInstance->Users.QuitUser(u, banreason);
	else
	{
		const std::string publicreason = Template::Replace(ServerInstance->Config->HideLines,
		{
			{ "reason", banreason },
			{ "type",   line      },
		});
		ServerInstance->Users.QuitUser(u, publicreason, &banreason);
	}

	if (bancache)
	{
		ServerInstance->Logs.Debug("BANCACHE", "Adding positive hit (" + line + ") for " + u->GetAddress());
		ServerInstance->BanCache.AddHit(u->GetAddress(), this->type, banreason, (this->duration > 0 ? (this->expiry - ServerInstance->Time()) : 0));
	}
}

bool KLine::Matches(User* u) const
{
	LocalUser* lu = IS_LOCAL(u);
	if (lu && lu->exempt)
		return false;

	if (InspIRCd::Match(u->ident, this->identmask, ascii_case_insensitive_map))
	{
		if (InspIRCd::MatchCIDR(u->GetRealHost(), this->hostmask, ascii_case_insensitive_map) ||
			InspIRCd::MatchCIDR(u->GetAddress(), this->hostmask, ascii_case_insensitive_map))
		{
			return true;
		}
	}

	return false;
}

void KLine::Apply(User* u)
{
	DefaultApply(u, "K", this->identmask ==  "*");
}

bool GLine::Matches(User* u) const
{
	LocalUser* lu = IS_LOCAL(u);
	if (lu && lu->exempt)
		return false;

	if (InspIRCd::Match(u->ident, this->identmask, ascii_case_insensitive_map))
	{
		if (InspIRCd::MatchCIDR(u->GetRealHost(), this->hostmask, ascii_case_insensitive_map) ||
			InspIRCd::MatchCIDR(u->GetAddress(), this->hostmask, ascii_case_insensitive_map))
		{
			return true;
		}
	}

	return false;
}

void GLine::Apply(User* u)
{
	DefaultApply(u, "G", this->identmask == "*");
}

bool ELine::Matches(User* u) const
{
	if (InspIRCd::Match(u->ident, this->identmask, ascii_case_insensitive_map))
	{
		if (InspIRCd::MatchCIDR(u->GetRealHost(), this->hostmask, ascii_case_insensitive_map) ||
			InspIRCd::MatchCIDR(u->GetAddress(), this->hostmask, ascii_case_insensitive_map))
		{
			return true;
		}
	}

	return false;
}

bool ZLine::Matches(User* u) const
{
	LocalUser* lu = IS_LOCAL(u);
	if (lu && lu->exempt)
		return false;

	return InspIRCd::MatchCIDR(u->GetAddress(), this->ipaddr);
}

void ZLine::Apply(User* u)
{
	DefaultApply(u, "Z", true);
}

bool QLine::Matches(User* u) const
{
	return InspIRCd::Match(u->nick, this->nick);
}

void QLine::Apply(User* u)
{
	/* Force to uuid on apply of Q-line, no need to disconnect anymore :) */
	u->WriteNumeric(RPL_SAVENICK, u->uuid, "Your nickname has been Q-lined.");
	u->ChangeNick(u->uuid);
}

bool ZLine::Matches(const std::string& str) const
{
	return InspIRCd::MatchCIDR(str, this->ipaddr);
}

bool QLine::Matches(const std::string& str) const
{
	return InspIRCd::Match(str, this->nick);
}

bool ELine::Matches(const std::string& str) const
{
	return InspIRCd::MatchCIDR(str, matchtext);
}

bool KLine::Matches(const std::string& str) const
{
	return InspIRCd::MatchCIDR(str, matchtext);
}

bool GLine::Matches(const std::string& str) const
{
	return InspIRCd::MatchCIDR(str, matchtext);
}

void ELine::OnAdd()
{
	/* When adding one E-line, only check the one E-line */
	for (auto* u : ServerInstance->Users.GetLocalUsers())
	{
		if (this->Matches(u))
			u->exempt = true;
	}
}

void XLine::DisplayExpiry()
{
	bool onechar = (type.length() == 1);
	ServerInstance->SNO.WriteToSnoMask('x', "Removing an expired {}{} on {} (set by {} {} ago): {}",
		type, (onechar ? "-line" : ""), Displayable(), source,
		Duration::ToString(ServerInstance->Time() - set_time), reason);
}

const std::string& ELine::Displayable() const
{
	return matchtext;
}

const std::string& KLine::Displayable() const
{
	return matchtext;
}

const std::string& GLine::Displayable() const
{
	return matchtext;
}

const std::string& ZLine::Displayable() const
{
	return ipaddr;
}

const std::string& QLine::Displayable() const
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

XLineFactory* XLineManager::GetFactory(const std::string& type)
{
	XLineFactMap::iterator n = line_factory.find(type);

	if (n == line_factory.end())
		return nullptr;

	return n->second;
}

void XLineManager::ExpireRemovedConfigLines(const std::string& type, const insp::flat_set<std::string>& configlines)
{
	// Nothing to do.
	if (lookup_lines.empty())
		return;

	ContainerIter xlines = lookup_lines.find(type);
	if (xlines == lookup_lines.end())
		return;

	for (LookupIter xline = xlines->second.begin(); xline != xlines->second.end(); )
	{
		LookupIter cachedxline = xline++;
		if (!cachedxline->second->from_config)
			continue;

		if (!configlines.count(cachedxline->second->Displayable()))
			ExpireLine(xlines, cachedxline);
	}
}
