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

/* $ModDesc: Allows for auditorium channels (+u) where nobody can see others joining and parting or the nick list */

class AuditoriumMode : public ModeHandler
{
 public:
	AuditoriumMode(Module* Creator) : ModeHandler(Creator, "auditorium", 'u', PARAM_NONE, MODETYPE_CHANNEL)
	{
		levelrequired = OP_VALUE;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (channel->IsModeSet(this) == adding)
			return MODEACTION_DENY;
		channel->SetMode(this, adding);
		return MODEACTION_ALLOW;
	}
};

class ModuleAuditorium : public Module
{
 private:
	AuditoriumMode aum;
	bool OpsVisible;
	bool OpsCanSee;
	bool OperCanSee;
 public:
	ModuleAuditorium() : aum(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(aum);

		OnRehash(NULL);

		Implementation eventlist[] = {
			I_OnUserJoin, I_OnUserPart, I_OnUserKick,
			I_OnBuildNeighborList, I_OnNamesListItem, I_OnSendWhoLine,
			I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	~ModuleAuditorium()
	{
	}

	void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("auditorium");
		OpsVisible = tag->getBool("opvisible");
		OpsCanSee = tag->getBool("opcansee");
		OperCanSee = tag->getBool("opercansee", true);
	}

	Version GetVersion()
	{
		return Version("Allows for auditorium channels (+u) where nobody can see others joining and parting or the nick list", VF_VENDOR);
	}

	/* Can they be seen by everyone? */
	bool IsVisible(Membership* memb)
	{
		if (!memb->chan->IsModeSet(&aum))
			return true;

		ModResult res = ServerInstance->OnCheckExemption(memb->user, memb->chan, "auditorium-vis");
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
		ModResult res = ServerInstance->OnCheckExemption(issuer,memb->chan,"auditorium-see");
		if (res.check(OpsCanSee && memb->chan->GetPrefixValue(issuer) >= OP_VALUE))
			return true;

		return false;
	}

	void OnNamesListItem(User* issuer, Membership* memb, std::string &prefixes, std::string &nick)
	{
		// Some module already hid this from being displayed, don't bother
		if (nick.empty())
			return;

		if (IsVisible(memb))
			return;

		if (CanSee(issuer, memb))
			return;

		nick.clear();
	}

	/** Build CUList for showing this join/part/kick */
	void BuildExcept(Membership* memb, CUList& excepts)
	{
		if (IsVisible(memb))
			return;

		const UserMembList* users = memb->chan->GetUsers();
		for(UserMembCIter i = users->begin(); i != users->end(); i++)
		{
			if (IS_LOCAL(i->first) && !CanSee(i->first, memb))
				excepts.insert(i->first);
		}
	}

	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& excepts)
	{
		BuildExcept(memb, excepts);
	}

	void OnUserPart(Membership* memb, std::string &partmessage, CUList& excepts)
	{
		BuildExcept(memb, excepts);
	}

	void OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& excepts)
	{
		BuildExcept(memb, excepts);
	}

	void OnBuildNeighborList(User* source, UserChanList &include, std::map<User*,bool> &exception)
	{
		UCListIter i = include.begin();
		while (i != include.end())
		{
			Channel* c = *i++;
			Membership* memb = c->GetUser(source);
			if (!memb || IsVisible(memb))
				continue;
			// this channel should not be considered when listing my neighbors
			include.erase(c);
			// however, that might hide me from ops that can see me...
			const UserMembList* users = c->GetUsers();
			for(UserMembCIter j = users->begin(); j != users->end(); j++)
			{
				if (IS_LOCAL(j->first) && CanSee(j->first, memb))
					exception[j->first] = true;
			}
		}
	}

	void OnSendWhoLine(User* source, const std::vector<std::string>& params, User* user, std::string& line)
	{
		Channel* channel = ServerInstance->FindChan(params[0]);
		if (!channel)
			return;
		Membership* memb = channel->GetUser(user);
		if ((!memb) || (IsVisible(memb)))
			return;
		if (CanSee(source, memb))
			return;
		line.clear();
	}
};

MODULE_INIT(ModuleAuditorium)
