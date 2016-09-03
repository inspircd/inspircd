/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Jens Voss <DukePyrolator@anope.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
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

class DelayJoinMode : public ModeHandler
{
	CUList empty;
 public:
	DelayJoinMode(Module* Parent) : ModeHandler(Parent, "delayjoin", 'D', PARAM_NONE, MODETYPE_CHANNEL)
	{
		levelrequired = OP_VALUE;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
};

class ModuleDelayJoin : public Module
{
	DelayJoinMode djm;
 public:
	LocalIntExt unjoined;
	ModuleDelayJoin()
		: djm(this)
		, unjoined("delayjoin", ExtensionItem::EXT_MEMBERSHIP, this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE;
	ModResult OnNamesListItem(User* issuer, Membership*, std::string& prefixes, std::string& nick) CXX11_OVERRIDE;
	void OnUserJoin(Membership*, bool, bool, CUList&) CXX11_OVERRIDE;
	void CleanUser(User* user);
	void OnUserPart(Membership*, std::string &partmessage, CUList&) CXX11_OVERRIDE;
	void OnUserKick(User* source, Membership*, const std::string &reason, CUList&) CXX11_OVERRIDE;
	void OnBuildNeighborList(User* source, IncludeChanList& include, std::map<User*, bool>& exception) CXX11_OVERRIDE;
	void OnText(User* user, void* dest, int target_type, const std::string &text, char status, CUList &exempt_list) CXX11_OVERRIDE;
	ModResult OnRawMode(User* user, Channel* channel, ModeHandler* mh, const std::string& param, bool adding) CXX11_OVERRIDE;
};

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
		const Channel::MemberMap& users = channel->GetUsers();
		for (Channel::MemberMap::const_iterator n = users.begin(); n != users.end(); ++n)
			creator->OnText(n->first, channel, TYPE_CHANNEL, "", 0, empty);
	}
	channel->SetMode(this, adding);
	return MODEACTION_ALLOW;
}

Version ModuleDelayJoin::GetVersion()
{
	return Version("Allows for delay-join channels (+D) where users don't appear to join until they speak", VF_VENDOR);
}

ModResult ModuleDelayJoin::OnNamesListItem(User* issuer, Membership* memb, std::string& prefixes, std::string& nick)
{
	/* don't prevent the user from seeing themself */
	if (issuer == memb->user)
		return MOD_RES_PASSTHRU;

	/* If the user is hidden by delayed join, hide them from the NAMES list */
	if (unjoined.get(memb))
		return MOD_RES_DENY;

	return MOD_RES_PASSTHRU;
}

static void populate(CUList& except, Membership* memb)
{
	const Channel::MemberMap& users = memb->chan->GetUsers();
	for (Channel::MemberMap::const_iterator i = users.begin(); i != users.end(); ++i)
	{
		if (i->first == memb->user || !IS_LOCAL(i->first))
			continue;
		except.insert(i->first);
	}
}

void ModuleDelayJoin::OnUserJoin(Membership* memb, bool sync, bool created, CUList& except)
{
	if (memb->chan->IsModeSet(djm))
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

void ModuleDelayJoin::OnBuildNeighborList(User* source, IncludeChanList& include, std::map<User*, bool>& exception)
{
	for (IncludeChanList::iterator i = include.begin(); i != include.end(); )
	{
		Membership* memb = *i;
		if (unjoined.get(memb))
			i = include.erase(i);
		else
			++i;
	}
}

void ModuleDelayJoin::OnText(User* user, void* dest, int target_type, const std::string &text, char status, CUList &exempt_list)
{
	if (target_type != TYPE_CHANNEL)
		return;

	Channel* channel = static_cast<Channel*>(dest);

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

/* make the user visible if he receives any mode change */
ModResult ModuleDelayJoin::OnRawMode(User* user, Channel* channel, ModeHandler* mh, const std::string& param, bool adding)
{
	if (!channel || param.empty())
		return MOD_RES_PASSTHRU;

	// If not a prefix mode then we got nothing to do here
	if (!mh->IsPrefixMode())
		return MOD_RES_PASSTHRU;

	User* dest;
	if (IS_LOCAL(user))
		dest = ServerInstance->FindNickOnly(param);
	else
		dest = ServerInstance->FindNick(param);

	if (!dest)
		return MOD_RES_PASSTHRU;

	Membership* memb = channel->GetUser(dest);
	if (memb && unjoined.set(memb, 0))
		channel->WriteAllExceptSender(dest, false, 0, "JOIN %s", channel->name.c_str());
	return MOD_RES_PASSTHRU;
}

MODULE_INIT(ModuleDelayJoin)
