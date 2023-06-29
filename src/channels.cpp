/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013-2014, 2016-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
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
#include "listmode.h"

namespace
{
	ChanModeReference ban(nullptr, "ban");
}

Channel::Channel(const std::string& cname, time_t ts)
	: Extensible(ExtensionType::CHANNEL)
	, name(cname)
	, age(ts)
{
	if (!ServerInstance->Channels.GetChans().emplace(cname, this).second)
		throw CoreException("Cannot create duplicate channel " + cname);
}

void Channel::SetMode(const ModeHandler* mh, bool on)
{
	if (mh && mh->GetId() != ModeParser::MODEID_MAX)
		modes[mh->GetId()] = on;
}

void Channel::SetTopic(User* u, const std::string& ntopic, time_t topicts, const std::string* setter)
{
	// Send a TOPIC message to the channel only if the new topic text differs
	if (this->topic != ntopic)
	{
		this->topic = ntopic;
		ClientProtocol::Messages::Topic topicmsg(u, this, this->topic);
		Write(ServerInstance->GetRFCEvents().topic, topicmsg);
	}

	// Always update setter and set time
	if (!setter)
		setter = ServerInstance->Config->FullHostInTopic ? &u->GetMask() : &u->nick;
	this->setby.assign(*setter, 0, ServerInstance->Config->Limits.GetMaxMask());
	this->topicset = topicts;

	FOREACH_MOD(OnPostTopicChange, (u, this, this->topic));
}

Membership* Channel::AddUser(User* user)
{
	std::pair<MemberMap::iterator, bool> ret = userlist.emplace(user, insp::aligned_storage<Membership>());
	if (!ret.second)
		return nullptr;

	Membership* memb = new(ret.first->second) Membership(user, this);
	return memb;
}

void Channel::DelUser(User* user)
{
	MemberMap::iterator it = userlist.find(user);
	if (it != userlist.end())
		DelUser(it);
}

void Channel::CheckDestroy()
{
	if (!userlist.empty())
		return;

	ModResult res;
	FIRST_MOD_RESULT(OnChannelPreDelete, res, (this));
	if (res == MOD_RES_DENY)
		return;

	// If the channel isn't in chanlist then it is already in the cull list, don't add it again
	ChannelMap::iterator iter = ServerInstance->Channels.GetChans().find(this->name);
	if ((iter == ServerInstance->Channels.GetChans().end()) || (iter->second != this))
		return;

	FOREACH_MOD(OnChannelDelete, (this));
	ServerInstance->Channels.GetChans().erase(iter);
	ServerInstance->GlobalCulls.AddItem(this);
}

void Channel::DelUser(const MemberMap::iterator& membiter)
{
	Membership* memb = membiter->second;
	memb->Cull();
	memb->~Membership();
	userlist.erase(membiter);

	// If this channel became empty then it should be removed
	CheckDestroy();
}

Membership* Channel::GetUser(User* user) const
{
	MemberMap::const_iterator i = userlist.find(user);
	if (i == userlist.end())
		return nullptr;
	return i->second;
}

void Channel::SetDefaultModes()
{
	ServerInstance->Logs.Debug("CHANNELS", "Setting default modes on {}: {}", name,
		ServerInstance->Config->DefaultModes);
	irc::spacesepstream list(ServerInstance->Config->DefaultModes);
	std::string modeseq;
	std::string parameter;

	list.GetToken(modeseq);

	for (const auto modechr : modeseq)
	{
		ModeHandler* mode = ServerInstance->Modes.FindMode(modechr, MODETYPE_CHANNEL);
		if (mode)
		{
			if (mode->IsPrefixMode())
				continue;

			if (mode->NeedsParam(true))
			{
				// If the parameter is missing or begins with a ':' then it's invalid
				if (!list.GetToken(parameter) || parameter[0] == ':')
					continue;
			}
			else
			{
				// The mode does not take a parameter.
				parameter.clear();
			}

			Modes::Change modechange(mode, true, parameter);
			mode->OnModeChange(ServerInstance->FakeClient, ServerInstance->FakeClient, this, modechange);
		}
	}
}

/*
 * add a channel to a user, creating the record for it if needed and linking
 * it to the user record
 */
Membership* Channel::JoinUser(LocalUser* user, std::string cname, bool override, const std::string& key)
{
	if (!user->IsFullyConnected())
	{
		ServerInstance->Logs.Debug("CHANNELS", "Attempted to join partially connected user " + user->uuid + " to channel " + cname);
		return nullptr;
	}

	// Crop channel name if it's too long
	if (cname.length() > ServerInstance->Config->Limits.MaxChannel)
		cname.resize(ServerInstance->Config->Limits.MaxChannel);

	auto* chan = ServerInstance->Channels.Find(cname);
	bool created_by_local = !chan; // Flag that will be passed to ForceJoin later
	std::string privs; // Prefix mode(letter)s to give to the joining user

	if (!chan)
	{
		privs = ServerInstance->Config->DefaultModes.substr(0, ServerInstance->Config->DefaultModes.find(' '));

		// Ask the modules whether they're ok with the join, pass NULL as Channel* as the channel is yet to be created
		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnUserPreJoin, MOD_RESULT, (user, nullptr, cname, privs, key, override));
		if (!override && MOD_RESULT == MOD_RES_DENY)
			return nullptr; // A module wasn't happy with the join, abort

		chan = new Channel(cname, ServerInstance->Time());
		// Set the default modes on the channel (<options:defaultmodes>)
		chan->SetDefaultModes();
	}
	else
	{
		/* Already on the channel */
		if (chan->HasUser(user))
			return nullptr;

		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnUserPreJoin, MOD_RESULT, (user, chan, cname, privs, key, override));

		// A module explicitly denied the join and (hopefully) generated a message
		// describing the situation, so we may stop here without sending anything
		if (!override && MOD_RESULT == MOD_RES_DENY)
			return nullptr;
	}

	// We figured that this join is allowed and also created the
	// channel if it didn't exist before, now do the actual join
	return chan->ForceJoin(user, &privs, false, created_by_local);
}

Membership* Channel::ForceJoin(User* user, const std::string* privs, bool bursting, bool created_by_local)
{
	if (IS_SERVER(user))
	{
		ServerInstance->Logs.Debug("CHANNELS", "Attempted to join server user " + user->uuid + " to channel " + this->name);
		return nullptr;
	}

	Membership* memb = this->AddUser(user);
	if (!memb)
		return nullptr; // Already on the channel

	user->chans.push_front(memb);

	if (privs)
	{
		// If the user was granted prefix modes (in the OnUserPreJoin hook, or they're a
		// remote user and their own server set the modes), then set them internally now
		for (const auto priv : *privs)
		{
			PrefixMode* mh = ServerInstance->Modes.FindPrefixMode(priv);
			if (mh)
			{
				// Set the mode on the user.
				Modes::Change modechange(mh, true, user->nick);
				mh->OnModeChange(ServerInstance->FakeClient, nullptr, this, modechange);
			}
		}
	}

	// Tell modules about this join, they have the chance now to populate except_list with users we won't send the JOIN (and possibly MODE) to
	CUList except_list;
	FOREACH_MOD(OnUserJoin, (memb, bursting, created_by_local, except_list));

	ClientProtocol::Events::Join joinevent(memb);
	this->Write(joinevent, 0, except_list);

	FOREACH_MOD(OnPostJoin, (memb));
	return memb;
}

bool Channel::IsBanned(User* user)
{
	ModResult result;
	FIRST_MOD_RESULT(OnCheckChannelBan, result, (user, this));

	if (result != MOD_RES_PASSTHRU)
		return (result == MOD_RES_DENY);

	ListModeBase* banlm = static_cast<ListModeBase*>(*ban);
	if (!banlm)
		return false;

	const ListModeBase::ModeList* bans = banlm->GetList(this);
	if (bans)
	{
		for (const auto& entry : *bans)
		{
			if (CheckBan(user, entry.mask))
				return true;
		}
	}
	return false;
}

bool Channel::CheckBan(User* user, const std::string& mask)
{
	ModResult result;
	FIRST_MOD_RESULT(OnCheckBan, result, (user, this, mask));
	if (result != MOD_RES_PASSTHRU)
		return (result == MOD_RES_DENY);

	std::string::size_type at = mask.find('@');
	if (at == std::string::npos)
		return false;

	const std::string prefix(mask, 0, at);
	if (!InspIRCd::Match(user->nick + "!" + user->GetDisplayedUser(), prefix) &&
		!InspIRCd::Match(user->nick + "!" + user->GetRealUser(), prefix))
	{
		// Neither the nick!user or nick!duser.
		return false;
	}

	const std::string suffix(mask, at + 1);
	return InspIRCd::Match(user->GetRealHost(), suffix) ||
		InspIRCd::Match(user->GetDisplayedHost(), suffix) ||
		InspIRCd::MatchCIDR(user->GetAddress(), suffix);
}

/* Channel::PartUser
 * Remove a channel from a users record, remove the reference to the Membership object
 * from the channel and destroy it.
 */
bool Channel::PartUser(User* user, const std::string& reason)
{
	MemberMap::iterator membiter = userlist.find(user);

	if (membiter == userlist.end())
		return false;

	Membership* memb = membiter->second;
	std::string partreason(reason);
	CUList except_list;
	FOREACH_MOD(OnUserPart, (memb, partreason, except_list));

	ClientProtocol::Messages::Part partmsg(memb, partreason);
	Write(ServerInstance->GetRFCEvents().part, partmsg, 0, except_list);

	// Remove this channel from the user's chanlist
	user->chans.erase(memb);
	// Remove the Membership from this channel's userlist and destroy it
	this->DelUser(membiter);

	return true;
}

void Channel::KickUser(User* src, const MemberMap::iterator& victimiter, const std::string& reason)
{
	Membership* memb = victimiter->second;
	CUList except_list;
	FOREACH_MOD(OnUserKick, (src, memb, reason, except_list));

	ClientProtocol::Messages::Kick kickmsg(src, memb, reason);
	Write(ServerInstance->GetRFCEvents().kick, kickmsg, 0, except_list);

	memb->user->chans.erase(memb);
	this->DelUser(victimiter);
}

void Channel::Write(ClientProtocol::Event& protoev, char status, const CUList& except_list) const
{
	ModeHandler::Rank minrank = 0;
	if (status)
	{
		PrefixMode* mh = ServerInstance->Modes.FindPrefix(status);
		if (mh)
			minrank = mh->GetPrefixRank();
	}

	for (const auto& [u, memb] : userlist)
	{
		LocalUser* user = IS_LOCAL(u);
		if ((user) && (!except_list.count(user)))
		{
			/* User doesn't have the status we're after */
			if (minrank && memb->GetRank() < minrank)
				continue;

			user->Send(protoev);
		}
	}
}

const char* Channel::ChanModes(bool showsecret)
{
	static std::string scratch;
	std::string sparam;

	scratch.clear();

	for (const auto& [_, mh] : ServerInstance->Modes.GetModes(MODETYPE_CHANNEL))
	{
		if (IsModeSet(mh))
		{
			scratch.push_back(mh->GetModeChar());

			ParamModeBase* pm = mh->IsParameterMode();
			if (!pm)
				continue;

			if (pm->IsParameterSecret() && !showsecret)
			{
				sparam += " <" + pm->name + ">";
			}
			else
			{
				sparam += ' ';
				pm->GetParameter(this, sparam);
			}
		}
	}

	scratch += sparam;
	return scratch.c_str();
}

void Channel::WriteNotice(const std::string& text, char status) const
{
	ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->FakeClient, this, text, MessageType::NOTICE, status);
	Write(ServerInstance->GetRFCEvents().privmsg, privmsg, status);
}

void Channel::WriteRemoteNotice(const std::string& text, char status) const
{
	WriteNotice(text, status);
	ServerInstance->PI->SendMessage(this, status, text, MessageType::NOTICE);
}

/* returns the status character for a given user on a channel, e.g. @ for op,
 * % for halfop etc. If the user has several modes set, the highest mode
 * the user has must be returned.
 */
char Membership::GetPrefixChar() const
{
	char pf = 0;
	ModeHandler::Rank bestrank = 0;

	for (const auto* mh : modes)
	{
		if (mh->GetPrefixRank() > bestrank && mh->GetPrefix())
		{
			bestrank = mh->GetPrefixRank();
			pf = mh->GetPrefix();
		}
	}
	return pf;
}

std::string Membership::GetAllPrefixChars() const
{
	std::string ret;
	ret.reserve(modes.size());
	for (const auto* mh : modes)
	{
		if (mh->GetPrefix())
			ret.push_back(mh->GetPrefix());
	}
	return ret;
}

std::string Membership::GetAllPrefixModes() const
{
	std::string ret;
	ret.reserve(modes.size());
	for (const auto* mh : modes)
	{
		if (mh->GetModeChar())
			ret.push_back(mh->GetModeChar());
	}
	return ret;
}

ModeHandler::Rank Channel::GetPrefixValue(User* user) const
{
	MemberMap::const_iterator m = userlist.find(user);
	if (m == userlist.end())
		return 0;
	return m->second->GetRank();
}

bool Membership::SetPrefix(PrefixMode* delta_mh, bool adding)
{
	if (adding)
		return modes.insert(delta_mh).second;
	else
		return modes.erase(delta_mh);
}

void Membership::WriteNotice(const std::string& text) const
{
	LocalUser* const localuser = IS_LOCAL(user);
	if (!localuser)
		return;

	ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->FakeClient, this->chan, text, MessageType::NOTICE);
	localuser->Send(ServerInstance->GetRFCEvents().privmsg, privmsg);
}
