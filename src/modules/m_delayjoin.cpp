/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Jens Voss <DukePyrolator@anope.org>
 *   Copyright (C) 2009 Matt Smith <dz@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
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
#include "clientprotocolevent.h"
#include "modules/ctctags.h"
#include "modules/ircv3_servertime.h"
#include "modules/names.h"
#include "modules/who.h"

class DelayJoinMode final
	: public SimpleChannelMode
{
private:
	IntExtItem& unjoined;
	IRCv3::ServerTime::API servertime;

public:
	DelayJoinMode(Module* Parent, IntExtItem& ext)
		: SimpleChannelMode(Parent, "delayjoin", 'D')
		, unjoined(ext)
		, servertime(Parent)
	{
		ranktoset = ranktounset = OP_VALUE;
	}

	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		if (SimpleChannelMode::OnModeChange(source, dest, channel, change))
		{
			/*
			 * Make all users visible, as +D is being removed. If we don't do this,
			 * they remain permanently invisible on this channel!
			 */
			for (const auto& [member, _] : channel->GetUsers())
				RevealUser(member, channel);
			return true;
		}
		return false;
	}

	void RevealUser(User* user, Channel* chan);
};

namespace
{

/** Hook handler for join client protocol events.
 * This allows us to block join protocol events completely, including all associated messages (e.g. MODE, away-notify AWAY).
 * This is not the same as OnUserJoin() because that runs only when a real join happens but this runs also when a module
 * such as hostcycle generates a join.
 */
class JoinHook final
	: public ClientProtocol::EventHook
{
private:
	const IntExtItem& unjoined;

public:
	JoinHook(Module* mod, const IntExtItem& unjoinedref)
		: ClientProtocol::EventHook(mod, "JOIN", 10)
		, unjoined(unjoinedref)
	{
	}

	ModResult OnPreEventSend(LocalUser* user, const ClientProtocol::Event& ev, ClientProtocol::MessageList& messagelist) override
	{
		const ClientProtocol::Events::Join& join = static_cast<const ClientProtocol::Events::Join&>(ev);
		const Membership* const memb = join.GetMember();
		const User* const u = memb->user;
		if ((unjoined.Get(memb)) && (u != user))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}
};

}

class ModuleDelayJoin final
	: public Module
	, public CTCTags::EventListener
	, public Names::EventListener
	, public Who::EventListener
	, public Who::VisibleEventListener
{
private:
	IntExtItem unjoined;
	JoinHook joinhook;
	DelayJoinMode djm;

	void PopulateExcepts(CUList& except, Membership* memb)
	{
		if (!unjoined.Get(memb))
			return;

		unjoined.Unset(memb);
		for (const auto& [member, _] : memb->chan->GetUsers())
		{
			if (member != memb->user && IS_LOCAL(member))
				except.insert(member);
		}
	}

public:
	ModuleDelayJoin()
		: Module(VF_VENDOR, "Adds channel mode D (delayjoin) which hides JOIN messages from users until they speak.")
		, CTCTags::EventListener(this)
		, Names::EventListener(this)
		, Who::EventListener(this)
		, Who::VisibleEventListener(this)
		, unjoined(this, "delayjoin", ExtensionType::MEMBERSHIP)
		, joinhook(this, unjoined)
		, djm(this, unjoined)
	{
	}

	ModResult OnNamesListItem(LocalUser* issuer, Membership*, std::string& prefixes, std::string& nick) override;
	ModResult OnWhoLine(const Who::Request& request, LocalUser* source, User* user, Membership* memb, Numeric::Numeric& numeric) override;
	ModResult OnWhoVisible(const Who::Request& request, LocalUser* source, Membership* memb) override;
	void OnUserJoin(Membership*, bool, bool, CUList&) override;
	void CleanUser(User* user);
	void OnUserPart(Membership*, std::string& partmessage, CUList&) override;
	void OnUserKick(User* source, Membership*, const std::string& reason, CUList&) override;
	void OnBuildNeighborList(User* source, User::NeighborList& include, User::NeighborExceptions& exception) override;
	void OnUserMessage(User* user, const MessageTarget& target, const MessageDetails& details) override;
	void OnUserTagMessage(User* user, const MessageTarget& target, const CTCTags::TagMessageDetails& details) override;
	ModResult OnRawMode(User* user, Channel* channel, const Modes::Change& change) override;
};

ModResult ModuleDelayJoin::OnNamesListItem(LocalUser* issuer, Membership* memb, std::string& prefixes, std::string& nick)
{
	/* don't prevent the user from seeing themself */
	if (issuer == memb->user)
		return MOD_RES_PASSTHRU;

	/* If the user is hidden by delayed join, hide them from the NAMES list */
	if (unjoined.Get(memb))
		return MOD_RES_DENY;

	return MOD_RES_PASSTHRU;
}

ModResult ModuleDelayJoin::OnWhoLine(const Who::Request& request, LocalUser* source, User* user, Membership* memb, Numeric::Numeric& numeric)
{
	// We don't need to do anything if they're not delayjoined.
	if (!memb || !unjoined.Get(memb))
		return MOD_RES_PASSTHRU;

	// Only show delayjoined users to others if the d flag has been specified.
	if (source != user && !request.flags['d'])
		return MOD_RES_DENY;

	// Add the < flag to mark the user as delayjoined.
	size_t flag_index;
	if (request.GetFieldIndex('f', flag_index))
		numeric.GetParams()[flag_index].push_back('<');
	return MOD_RES_PASSTHRU;
}

ModResult ModuleDelayJoin::OnWhoVisible(const Who::Request& request, LocalUser* source, Membership* memb)
{
	// A WHO request is visible if:
	// 1. The source is the user.
	// 2. The user specified the delayjoin `d` flag.
	// 3. The user is not delayjoined.
	return source == memb->user || request.flags['d'] || !unjoined.Get(memb) ? MOD_RES_PASSTHRU : MOD_RES_DENY;
}

void ModuleDelayJoin::OnUserJoin(Membership* memb, bool sync, bool created, CUList& except)
{
	if (memb->chan->IsModeSet(djm))
		unjoined.Set(memb, ServerInstance->Time());
}

void ModuleDelayJoin::OnUserPart(Membership* memb, std::string& partmessage, CUList& except)
{
	PopulateExcepts(except, memb);
}

void ModuleDelayJoin::OnUserKick(User* source, Membership* memb, const std::string& reason, CUList& except)
{
	PopulateExcepts(except, memb);
}

void ModuleDelayJoin::OnBuildNeighborList(User* source, User::NeighborList& include, User::NeighborExceptions& exception)
{
	for (User::NeighborList::iterator i = include.begin(); i != include.end(); )
	{
		Membership* memb = *i;
		if (unjoined.Get(memb))
			i = include.erase(i);
		else
			++i;
	}
}

void ModuleDelayJoin::OnUserTagMessage(User* user, const MessageTarget& target, const CTCTags::TagMessageDetails& details)
{
	if (target.type != MessageTarget::TYPE_CHANNEL)
		return;

	auto* channel = target.Get<Channel>();
	djm.RevealUser(user, channel);
}

void ModuleDelayJoin::OnUserMessage(User* user, const MessageTarget& target, const MessageDetails& details)
{
	if (target.type != MessageTarget::TYPE_CHANNEL)
		return;

	auto* channel = target.Get<Channel>();
	djm.RevealUser(user, channel);
}

void DelayJoinMode::RevealUser(User* user, Channel* chan)
{
	Membership* memb = chan->GetUser(user);
	if (!memb)
		return;

	time_t jointime = unjoined.Get(memb);
	if (!jointime)
		return;

	/* Display the join to everyone else (the user who joined got it earlier) */
	unjoined.Unset(memb);
	CUList except_list;
	except_list.insert(user);
	ClientProtocol::Events::Join joinevent(memb);
	if (servertime)
		servertime->Set(joinevent, jointime);
	chan->Write(joinevent, 0, except_list);
}

/* make the user visible if they receive any mode change */
ModResult ModuleDelayJoin::OnRawMode(User* user, Channel* channel, const Modes::Change& change)
{
	if (!channel || change.param.empty())
		return MOD_RES_PASSTHRU;

	// If not a prefix mode then we got nothing to do here
	if (!change.mh->IsPrefixMode())
		return MOD_RES_PASSTHRU;

	User* dest;
	if (IS_LOCAL(user))
		dest = ServerInstance->Users.FindNick(change.param);
	else
		dest = ServerInstance->Users.Find(change.param);

	if (!dest)
		return MOD_RES_PASSTHRU;

	djm.RevealUser(dest, channel);
	return MOD_RES_PASSTHRU;
}

MODULE_INIT(ModuleDelayJoin)
