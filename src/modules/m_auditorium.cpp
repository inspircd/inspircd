/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
#include "modules/exemption.h"

class AuditoriumMode : public SimpleChannelModeHandler
{
 public:
	AuditoriumMode(Module* Creator) : SimpleChannelModeHandler(Creator, "auditorium", 'u')
	{
		levelrequired = OP_VALUE;
	}
};

class ModuleAuditorium : public Module
{
	CheckExemption::EventProvider exemptionprov;
	AuditoriumMode aum;
	bool OpsVisible;
	bool OpsCanSee;
	bool OperCanSee;

 public:
	ModuleAuditorium()
		: exemptionprov(this)
		, aum(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("auditorium");
		OpsVisible = tag->getBool("opvisible");
		OpsCanSee = tag->getBool("opcansee");
		OperCanSee = tag->getBool("opercansee", true);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows for auditorium channels (+u) where nobody can see others joining and parting or the nick list", VF_VENDOR);
	}

	/* Can they be seen by everyone? */
	bool IsVisible(Membership* memb)
	{
		if (!memb->chan->IsModeSet(&aum))
			return true;

		ModResult res;
		FIRST_MOD_RESULT_CUSTOM(exemptionprov, CheckExemption::EventListener, OnCheckExemption, res, (memb->user, memb->chan, "auditorium-vis"));
		return res.check(OpsVisible && memb->getRank() >= OP_VALUE);
	}

	/* Can they see this specific membership? */
	bool CanSee(User* issuer, Membership* memb)
	{
		// If user is oper and operoverride is on, don't touch the list
		if (OperCanSee && issuer->HasPrivPermission("channels/auspex"))
			return true;

		// You can always see yourself
		if (issuer == memb->user)
			return true;

		// Can you see the list by permission?
		ModResult res;
		FIRST_MOD_RESULT_CUSTOM(exemptionprov, CheckExemption::EventListener, OnCheckExemption, res, (issuer, memb->chan, "auditorium-see"));
		if (res.check(OpsCanSee && memb->chan->GetPrefixValue(issuer) >= OP_VALUE))
			return true;

		return false;
	}

	ModResult OnNamesListItem(User* issuer, Membership* memb, std::string& prefixes, std::string& nick) CXX11_OVERRIDE
	{
		if (IsVisible(memb))
			return MOD_RES_PASSTHRU;

		if (CanSee(issuer, memb))
			return MOD_RES_PASSTHRU;

		// Don't display this user in the NAMES list
		return MOD_RES_DENY;
	}

	/** Build CUList for showing this join/part/kick */
	void BuildExcept(Membership* memb, CUList& excepts)
	{
		if (IsVisible(memb))
			return;

		const Channel::MemberMap& users = memb->chan->GetUsers();
		for (Channel::MemberMap::const_iterator i = users.begin(); i != users.end(); ++i)
		{
			if (IS_LOCAL(i->first) && !CanSee(i->first, memb))
				excepts.insert(i->first);
		}
	}

	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& excepts) CXX11_OVERRIDE
	{
		BuildExcept(memb, excepts);
	}

	void OnUserPart(Membership* memb, std::string &partmessage, CUList& excepts) CXX11_OVERRIDE
	{
		BuildExcept(memb, excepts);
	}

	void OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& excepts) CXX11_OVERRIDE
	{
		BuildExcept(memb, excepts);
	}

	void OnBuildNeighborList(User* source, IncludeChanList& include, std::map<User*, bool>& exception) CXX11_OVERRIDE
	{
		for (IncludeChanList::iterator i = include.begin(); i != include.end(); )
		{
			Membership* memb = *i;
			if (IsVisible(memb))
			{
				++i;
				continue;
			}

			// this channel should not be considered when listing my neighbors
			i = include.erase(i);
			// however, that might hide me from ops that can see me...
			const Channel::MemberMap& users = memb->chan->GetUsers();
			for(Channel::MemberMap::const_iterator j = users.begin(); j != users.end(); ++j)
			{
				if (IS_LOCAL(j->first) && CanSee(j->first, memb))
					exception[j->first] = true;
			}
		}
	}

	ModResult OnSendWhoLine(User* source, const std::vector<std::string>& params, User* user, Membership* memb, Numeric::Numeric& numeric) CXX11_OVERRIDE
	{
		if (!memb)
			return MOD_RES_PASSTHRU;
		if (IsVisible(memb))
			return MOD_RES_PASSTHRU;
		if (CanSee(source, memb))
			return MOD_RES_PASSTHRU;
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleAuditorium)
