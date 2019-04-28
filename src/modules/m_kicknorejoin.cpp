/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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
#include "modules/invite.h"

enum
{
	// From RFC 2182.
	ERR_UNAVAILRESOURCE = 437
};


class KickRejoinData
{
	struct KickedUser
	{
		std::string uuid;
		time_t expire;

		KickedUser(User* user, unsigned int Delay)
			: uuid(user->uuid)
			, expire(ServerInstance->Time() + Delay)
		{
		}
	};

	typedef std::vector<KickedUser> KickedList;

	mutable KickedList kicked;

 public:
	const unsigned int delay;

	KickRejoinData(unsigned int Delay) : delay(Delay) { }

	bool canjoin(LocalUser* user) const
	{
		for (KickedList::iterator i = kicked.begin(); i != kicked.end(); )
		{
			KickedUser& rec = *i;
			if (rec.expire > ServerInstance->Time())
			{
				if (rec.uuid == user->uuid)
					return false;
				++i;
			}
			else
			{
				// Expired record, remove.
				stdalgo::vector::swaperase(kicked, i);
				if (kicked.empty())
					break;
			}
		}
		return true;
	}

	void add(User* user)
	{
		// One user can be in the list multiple times if the user gets kicked, force joins
		// (skipping OnUserPreJoin) and gets kicked again, but that's okay because canjoin()
		// works correctly in this case as well
		kicked.push_back(KickedUser(user, delay));
	}
};

/** Handles channel mode +J
 */
class KickRejoin : public ParamMode<KickRejoin, SimpleExtItem<KickRejoinData> >
{
	const unsigned int max;
 public:
	KickRejoin(Module* Creator)
		: ParamMode<KickRejoin, SimpleExtItem<KickRejoinData> >(Creator, "kicknorejoin", 'J')
		, max(60)
	{
	}

	ModeAction OnSet(User* source, Channel* channel, std::string& parameter) CXX11_OVERRIDE
	{
		unsigned int v = ConvToNum<unsigned int>(parameter);
		if (v <= 0)
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return MODEACTION_DENY;
		}

		if (IS_LOCAL(source) && v > max)
			v = max;

		ext.set(channel, new KickRejoinData(v));
		return MODEACTION_ALLOW;
	}

	void SerializeParam(Channel* chan, const KickRejoinData* krd, std::string& out)
	{
		out.append(ConvToStr(krd->delay));
	}

	std::string GetModuleSettings() const
	{
		return ConvToStr(max);
	}
};

class ModuleKickNoRejoin : public Module
{
	KickRejoin kr;
	Invite::API invapi;

public:
	ModuleKickNoRejoin()
		: kr(this)
		, invapi(this)
	{
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		if (chan)
		{
			const KickRejoinData* data = kr.ext.get(chan);
			if ((data) && !invapi->IsInvited(user, chan) && (!data->canjoin(user)))
			{
				user->WriteNumeric(ERR_UNAVAILRESOURCE, chan, InspIRCd::Format("You must wait %u seconds after being kicked to rejoin (+J is set)", data->delay));
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& excepts) CXX11_OVERRIDE
	{
		if ((!IS_LOCAL(memb->user)) || (source == memb->user))
			return;

		KickRejoinData* data = kr.ext.get(memb->chan);
		if (data)
		{
			data->add(memb->user);
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel mode +J, delays rejoins after kicks", VF_VENDOR | VF_COMMON, kr.GetModuleSettings());
	}
};

MODULE_INIT(ModuleKickNoRejoin)
