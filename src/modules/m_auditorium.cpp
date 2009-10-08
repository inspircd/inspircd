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
		if (channel->IsModeSet('u') != adding)
		{
			channel->SetMode('u', adding);
			return MODEACTION_ALLOW;
		}
		else
		{
			return MODEACTION_DENY;
		}
	}
};

class ModuleAuditorium : public Module
{
 private:
	AuditoriumMode aum;
	bool ShowOps;
	bool OperOverride;
 public:
	ModuleAuditorium()
		: aum(this)
	{
		if (!ServerInstance->Modes->AddMode(&aum))
			throw ModuleException("Could not add new modes!");

		OnRehash(NULL);

		Implementation eventlist[] = { I_OnUserJoin, I_OnUserPart, I_OnUserKick, I_OnBuildNeighborList, I_OnNamesListItem, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 6);

	}

	~ModuleAuditorium()
	{
	}

	void OnRehash(User* user)
	{
		ConfigReader conf;
		ShowOps = conf.ReadFlag("auditorium", "showops", 0);
		OperOverride = conf.ReadFlag("auditorium", "operoverride", 0);
	}

	Version GetVersion()
	{
		return Version("Allows for auditorium channels (+u) where nobody can see others joining and parting or the nick list", VF_COMMON | VF_VENDOR, API_VERSION);
	}

	void OnNamesListItem(User* issuer, Membership* memb, std::string &prefixes, std::string &nick)
	{
		if (!memb->chan->IsModeSet('u'))
			return;

		/* Some module hid this from being displayed, dont bother */
		if (nick.empty())
			return;

		/* If user is oper and operoverride is on, don't touch the list */
		if (OperOverride && issuer->HasPrivPermission("channels/auspex"))
			return;

		if (ShowOps && (issuer != memb->user) && (memb->getRank() < OP_VALUE))
		{
			/* Showops is set, hide all non-ops from the user, except themselves */
			nick.clear();
			return;
		}

		if (!ShowOps && (issuer != memb->user))
		{
			/* ShowOps is not set, hide everyone except the user whos requesting NAMES */
			nick.clear();
			return;
		}
	}

	void BuildExcept(Membership* memb, CUList& excepts)
	{
		if (!memb->chan->IsModeSet('u'))
			return;
		if (ShowOps && memb->getRank() >= OP_VALUE)
			return;

		const UserMembList* users = memb->chan->GetUsers();
		for(UserMembCIter i = users->begin(); i != users->end(); i++)
		{
			if (i->first == memb->user || !IS_LOCAL(i->first))
				continue;
			if (ShowOps && i->second->getRank() >= OP_VALUE)
				continue;
			if (OperOverride && i->first->HasPrivPermission("channels/auspex"))
				continue;
			// This is a different user in the channel, local, and not op/oper
			// so, hide the join from them
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
			if (c->IsModeSet('u'))
				include.erase(c);
		}
	}
};

MODULE_INIT(ModuleAuditorium)
