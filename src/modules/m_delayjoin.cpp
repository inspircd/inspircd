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
#include "modules/ctctags.h"
#include "modules/names.h"

class DelayJoinMode : public ModeHandler
{
 private:
	LocalIntExt& unjoined;

 public:
	DelayJoinMode(Module* Parent, LocalIntExt& ext)
		: ModeHandler(Parent, "delayjoin", 'D', PARAM_NONE, MODETYPE_CHANNEL)
		, unjoined(ext)
	{
		ranktoset = ranktounset = OP_VALUE;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string& parameter, bool adding) CXX11_OVERRIDE;
	void RevealUser(User* user, Channel* chan);
};


namespace
{

/** Hook handler for join client protocol events.
 * This allows us to block join protocol events completely, including all associated messages (e.g. MODE, away-notify AWAY).
 * This is not the same as OnUserJoin() because that runs only when a real join happens but this runs also when a module
 * such as hostcycle generates a join.
 */
class JoinHook : public ClientProtocol::EventHook
{
	const LocalIntExt& unjoined;

 public:
	JoinHook(Module* mod, const LocalIntExt& unjoinedref)
		: ClientProtocol::EventHook(mod, "JOIN", 10)
		, unjoined(unjoinedref)
	{
	}

	ModResult OnPreEventSend(LocalUser* user, const ClientProtocol::Event& ev, ClientProtocol::MessageList& messagelist) CXX11_OVERRIDE
	{
		const ClientProtocol::Events::Join& join = static_cast<const ClientProtocol::Events::Join&>(ev);
		const Membership* const memb = join.GetMember();
		const User* const u = memb->user;
		if ((unjoined.get(memb)) && (u != user))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}
};

}

class ModuleDelayJoin 
	: public Module
	, public CTCTags::EventListener
	, public Names::EventListener
{
 public:
	LocalIntExt unjoined;
	JoinHook joinhook;
	DelayJoinMode djm;

	ModuleDelayJoin()
		: CTCTags::EventListener(this)
		, Names::EventListener(this)
		, unjoined("delayjoin", ExtensionItem::EXT_MEMBERSHIP, this)
		, joinhook(this, unjoined)
		, djm(this, unjoined)
	{
	}

	Version GetVersion() CXX11_OVERRIDE;
	ModResult OnNamesListItem(LocalUser* issuer, Membership*, std::string& prefixes, std::string& nick) CXX11_OVERRIDE;
	void OnUserJoin(Membership*, bool, bool, CUList&) CXX11_OVERRIDE;
	void CleanUser(User* user);
	void OnUserPart(Membership*, std::string &partmessage, CUList&) CXX11_OVERRIDE;
	void OnUserKick(User* source, Membership*, const std::string &reason, CUList&) CXX11_OVERRIDE;
	void OnBuildNeighborList(User* source, IncludeChanList& include, std::map<User*, bool>& exception) CXX11_OVERRIDE;
	void OnUserMessage(User* user, const MessageTarget& target, const MessageDetails& details) CXX11_OVERRIDE;
	void OnUserTagMessage(User* user, const MessageTarget& target, const CTCTags::TagMessageDetails& details) CXX11_OVERRIDE;
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
			RevealUser(n->first, channel);
	}
	channel->SetMode(this, adding);
	return MODEACTION_ALLOW;
}

Version ModuleDelayJoin::GetVersion()
{
	return Version("Provides channel mode +D, delay-join, users don't appear as joined to others until they speak", VF_VENDOR);
}

ModResult ModuleDelayJoin::OnNamesListItem(LocalUser* issuer, Membership* memb, std::string& prefixes, std::string& nick)
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
		unjoined.set(memb, 1);
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

void ModuleDelayJoin::OnUserTagMessage(User* user, const MessageTarget& target, const CTCTags::TagMessageDetails& details)
{
	if (target.type != MessageTarget::TYPE_CHANNEL)
		return;

	Channel* channel = target.Get<Channel>();
	djm.RevealUser(user, channel);
}

void ModuleDelayJoin::OnUserMessage(User* user, const MessageTarget& target, const MessageDetails& details)
{
	if (target.type != MessageTarget::TYPE_CHANNEL)
		return;

	Channel* channel = target.Get<Channel>();
	djm.RevealUser(user, channel);
}

void DelayJoinMode::RevealUser(User* user, Channel* chan)
{
	Membership* memb = chan->GetUser(user);
	if (!memb || !unjoined.set(memb, 0))
		return;

	/* Display the join to everyone else (the user who joined got it earlier) */
	CUList except_list;
	except_list.insert(user);
	ClientProtocol::Events::Join joinevent(memb);
	chan->Write(joinevent, 0, except_list);
}

/* make the user visible if they receive any mode change */
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

	djm.RevealUser(dest, channel);
	return MOD_RES_PASSTHRU;
}

MODULE_INIT(ModuleDelayJoin)
