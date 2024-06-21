/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 A_D
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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
#include "extension.h"
#include "modules/invite.h"
#include "numerichelper.h"

class KickRejoinData final
{
	struct KickedUser final
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

	KickRejoinData(unsigned int Delay)
		: delay(Delay)
	{
	}

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
		kicked.emplace_back(user, delay);
	}
};

/** Handles channel mode +J
 */
class KickRejoin final
	: public ParamMode<KickRejoin, SimpleExtItem<KickRejoinData>>
{
public:
	const unsigned int max = 60;

	KickRejoin(Module* Creator)
		: ParamMode<KickRejoin, SimpleExtItem<KickRejoinData>>(Creator, "kicknorejoin", 'J')
	{
		syntax = "<seconds>";
	}

	bool OnSet(User* source, Channel* channel, std::string& parameter) override
	{
		unsigned int v = ConvToNum<unsigned int>(parameter);
		if (v <= 0)
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return false;
		}

		if (IS_LOCAL(source) && v > max)
			v = max;

		ext.Set(channel, v);
		return true;
	}

	void SerializeParam(Channel* chan, const KickRejoinData* krd, std::string& out)
	{
		out.append(ConvToStr(krd->delay));
	}
};

class ModuleKickNoRejoin final
	: public Module
{
	KickRejoin kr;
	Invite::API invapi;

public:
	ModuleKickNoRejoin()
		: Module(VF_VENDOR | VF_COMMON, "Adds channel mode J (kicknorejoin) which prevents users from rejoining after being kicked from a channel.")
		, kr(this)
		, invapi(this)
	{
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
	{
		if (!override && chan)
		{
			const KickRejoinData* data = kr.ext.Get(chan);
			if ((data) && !invapi->IsInvited(user, chan) && (!data->canjoin(user)))
			{
				user->WriteNumeric(ERR_UNAVAILRESOURCE, chan->name, INSP_FORMAT("You must wait {} seconds after being kicked to rejoin (+{} is set)",
					data->delay, kr.GetModeChar()));
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void OnUserKick(User* source, Membership* memb, const std::string& reason, CUList& excepts) override
	{
		if ((!IS_LOCAL(memb->user)) || (source == memb->user))
			return;

		KickRejoinData* data = kr.ext.Get(memb->chan);
		if (data)
		{
			data->add(memb->user);
		}
	}

	void GetLinkData(LinkData& data, std::string& compatdata) override
	{
		data["max-time"] = compatdata = ConvToStr(kr.max);
	}
};

MODULE_INIT(ModuleKickNoRejoin)
