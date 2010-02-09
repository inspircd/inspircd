/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <stdarg.h>

class DelayJoinMode : public ModeHandler
{
 private:
	CUList empty;
 public:
	DelayJoinMode(Module* Parent) : ModeHandler(Parent, "delayjoin", 'D', PARAM_NONE, MODETYPE_CHANNEL)
	{
		levelrequired = OP_VALUE;
		fixed_letter = false;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
};

class ModuleDelayJoin : public Module
{
	DelayJoinMode djm;
 public:
	LocalIntExt unjoined;
	ModuleDelayJoin() : djm(this), unjoined("delayjoin", this)
	{
		if (!ServerInstance->Modes->AddMode(&djm))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserJoin, I_OnUserPart, I_OnUserKick, I_OnBuildNeighborList, I_OnNamesListItem, I_OnText };
		ServerInstance->Modules->Attach(eventlist, this, 6);
	}
	~ModuleDelayJoin();
	Version GetVersion();
	void OnNamesListItem(User* issuer, Membership*, std::string &prefixes, std::string &nick);
	void OnUserJoin(Membership*, bool, bool, CUList&);
	void CleanUser(User* user);
	void OnUserPart(Membership*, std::string &partmessage, CUList&);
	void OnUserKick(User* source, Membership*, const std::string &reason, CUList&);
	void OnBuildNeighborList(User* source, UserChanList &include, std::map<User*,bool> &exception);
	void OnText(User* user, void* dest, int target_type, const std::string &text, char status, CUList &exempt_list);
};

/* $ModDesc: Allows for delay-join channels (+D) where users dont appear to join until they speak */

ModeAction DelayJoinMode::OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
{
	/* no change */
	if (channel->IsModeSet(this) == adding)
		return MODEACTION_DENY;

	if (!adding)
	{
		/*
		 * Make all users visible, as +D is being removed. If we don't do this,
		 * they remain permanently invisible on this channel!
		 */
		const UserMembList* names = channel->GetUsers();
		for (UserMembCIter n = names->begin(); n != names->end(); ++n)
			creator->OnText(n->first, channel, TYPE_CHANNEL, "", 0, empty);
	}
	channel->SetMode(this, adding);
	return MODEACTION_ALLOW;
}

ModuleDelayJoin::~ModuleDelayJoin()
{
}

Version ModuleDelayJoin::GetVersion()
{
	return Version("Allows for delay-join channels (+D) where users dont appear to join until they speak", VF_VENDOR);
}

void ModuleDelayJoin::OnNamesListItem(User* issuer, Membership* memb, std::string &prefixes, std::string &nick)
{
	/* don't prevent the user from seeing themself */
	if (issuer == memb->user)
		return;

	/* If the user is hidden by delayed join, hide them from the NAMES list */
	if (unjoined.get(memb))
		nick.clear();
}

static void populate(CUList& except, Membership* memb)
{
	const UserMembList* users = memb->chan->GetUsers();
	for(UserMembCIter i = users->begin(); i != users->end(); i++)
	{
		if (i->first == memb->user || !IS_LOCAL(i->first))
			continue;
		except.insert(i->first);
	}
}

void ModuleDelayJoin::OnUserJoin(Membership* memb, bool sync, bool created, CUList& except)
{
	if (memb->chan->IsModeSet(&djm))
	{
		unjoined.set(memb, 1);
		populate(except, memb);
	}
}

void ModuleDelayJoin::OnUserPart(Membership* memb, std::string &partmessage, CUList& except)
{
	if (unjoined.set(memb, 0))
		populate(except, memb);
}

void ModuleDelayJoin::OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& except)
{
	if (unjoined.set(memb, 0))
		populate(except, memb);
}

void ModuleDelayJoin::OnBuildNeighborList(User* source, UserChanList &include, std::map<User*,bool> &exception)
{
	UCListIter i = include.begin();
	while (i != include.end())
	{
		Channel* c = *i++;
		Membership* memb = c->GetUser(source);
		if (memb && unjoined.get(memb))
			include.erase(c);
	}
}

void ModuleDelayJoin::OnText(User* user, void* dest, int target_type, const std::string &text, char status, CUList &exempt_list)
{
	/* Server origin */
	if (!user)
		return;

	if (target_type != TYPE_CHANNEL)
		return;

	Channel* channel = (Channel*) dest;

	Membership* memb = channel->GetUser(user);
	if (!memb || !unjoined.set(memb, 0))
		return;

	/* Display the join to everyone else (the user who joined got it earlier) */
	channel->WriteAllExceptSender(user, false, 0, "JOIN %s", channel->name.c_str());

	std::string ms = memb->modes;
	for(unsigned int i=0; i < memb->modes.length(); i++)
		ms.append(" ").append(user->nick);

	if (ms.length() > 0)
		channel->WriteAllExceptSender(user, false, 0, "MODE %s +%s", channel->name.c_str(), ms.c_str());
}

MODULE_INIT(ModuleDelayJoin)

