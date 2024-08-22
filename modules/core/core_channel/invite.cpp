/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019, 2021-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
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

#include "invite.h"

class InviteExpireTimer final
	: public Timer
{
	Invite::Invite* const inv;

	bool Tick() override;

public:
	InviteExpireTimer(Invite::Invite* invite, time_t timeout);
};

static Invite::APIImpl* apiimpl;

void RemoveInvite(Invite::Invite* inv, bool remove_user, bool remove_chan)
{
	apiimpl->Destruct(inv, remove_user, remove_chan);
}

void UnserializeInvite(LocalUser* user, const std::string& str)
{
	apiimpl->Unserialize(user, str);
}

Invite::APIBase::APIBase(Module* parent)
	: DataProvider(parent, "core_channel_invite")
{
}

Invite::APIImpl::APIImpl(Module* parent)
	: APIBase(parent)
	, userext(parent, "invite_user")
	, chanext(parent, "invite_chan")
{
	apiimpl = this;
}

void Invite::APIImpl::Destruct(Invite* inv, bool remove_user, bool remove_chan)
{
	Store<LocalUser>* ustore = userext.Get(inv->user);
	if (ustore)
	{
		ustore->invites.erase(inv);
		if ((remove_user) && (ustore->invites.empty()))
			userext.Unset(inv->user);
	}

	Store<Channel>* cstore = chanext.Get(inv->chan);
	if (cstore)
	{
		cstore->invites.erase(inv);
		if ((remove_chan) && (cstore->invites.empty()))
			chanext.Unset(inv->chan);
	}

	delete inv;
}

bool Invite::APIImpl::Remove(LocalUser* user, Channel* chan)
{
	Invite* inv = Find(user, chan);
	if (inv)
	{
		Destruct(inv);
		return true;
	}
	return false;
}

void Invite::APIImpl::Create(LocalUser* user, Channel* chan, time_t timeout)
{
	if ((timeout != 0) && (ServerInstance->Time() >= timeout))
		// Expired, don't bother
		return;

	ServerInstance->Logs.Debug(MODNAME, "Invite::APIImpl::Create(): user={} chan={} timeout={}",
		user->uuid, chan->name, timeout);

	Invite* inv = Find(user, chan);
	if (inv)
	{
		// We only ever extend invites, so nothing to do if the existing one is not timed
		if (!inv->IsTimed())
			return;

		ServerInstance->Logs.Debug(MODNAME, "Invite::APIImpl::Create(): changing expiration in {}",
			FMT_PTR(inv));
		if (timeout == 0)
		{
			// Convert timed invite to non-expiring
			stdalgo::delete_zero(inv->expiretimer);
		}
		else if (inv->expiretimer->GetTrigger() >= ServerInstance->Time() + timeout)
		{
			// New expiration time is further than the current, extend the expiration
			inv->expiretimer->SetInterval(timeout - ServerInstance->Time());
		}
	}
	else
	{
		inv = new Invite(user, chan);
		if (timeout)
		{
			inv->expiretimer = new InviteExpireTimer(inv, timeout - ServerInstance->Time());
			ServerInstance->Timers.AddTimer(inv->expiretimer);
		}

		userext.Get(user, true)->invites.push_front(inv);
		chanext.Get(chan, true)->invites.push_front(inv);
		ServerInstance->Logs.Debug(MODNAME, "Invite::APIImpl::Create(): created new Invite {}",
			FMT_PTR(inv));
	}
}

Invite::Invite* Invite::APIImpl::Find(LocalUser* user, Channel* chan)
{
	const List* list = APIImpl::GetList(user);
	if (!list)
		return nullptr;

	for (auto* inv : *list)
	{
		if (inv->chan == chan)
			return inv;
	}

	return nullptr;
}

const Invite::List* Invite::APIImpl::GetList(LocalUser* user)
{
	Store<LocalUser>* list = userext.Get(user);
	if (list)
		return &list->invites;
	return nullptr;
}

void Invite::APIImpl::Unserialize(LocalUser* user, const std::string& value)
{
	irc::spacesepstream ss(value);
	for (std::string channame, exptime; (ss.GetToken(channame) && ss.GetToken(exptime)); )
	{
		auto* chan = ServerInstance->Channels.Find(channame);
		if (chan)
			Create(user, chan, ConvToNum<time_t>(exptime));
	}
}

Invite::Invite::Invite(LocalUser* u, Channel* c)
	: user(u)
	, chan(c)
{
}

Invite::Invite::~Invite()
{
	delete expiretimer;
	ServerInstance->Logs.Debug(MODNAME, "Invite::~ {}", FMT_PTR(this));
}

void Invite::Invite::Serialize(bool human, bool show_chans, std::string& out)
{
	if (show_chans)
		out.append(this->chan->name);
	else
		out.append(human ? user->nick : user->uuid);
	out.push_back(' ');

	if (expiretimer)
		out.append(ConvToStr(expiretimer->GetTrigger()));
	else
		out.push_back('0');
	out.push_back(' ');
}

InviteExpireTimer::InviteExpireTimer(Invite::Invite* invite, time_t timeout)
	: Timer(timeout, false)
	, inv(invite)
{
}

bool InviteExpireTimer::Tick()
{
	ServerInstance->Logs.Debug(MODNAME, "InviteExpireTimer::Tick(): expired {}", FMT_PTR(inv));
	apiimpl->Destruct(inv);
	return false;
}
