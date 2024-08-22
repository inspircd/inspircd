/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2016, 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <brain@inspircd.org>
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
#include "modules/server.h"
#include "numerichelper.h"

// The number of seconds the channel will be closed for.
static unsigned int duration;

/** Holds settings and state associated with channel mode +j
 */
class joinfloodsettings final
{
public:
	unsigned int secs;
	unsigned int joins;
	time_t reset;
	time_t unlocktime = 0;
	unsigned int counter = 0;

	joinfloodsettings(unsigned int b, unsigned int c)
		: secs(b)
		, joins(c)
	{
		reset = ServerInstance->Time() + secs;
	}

	void addjoin()
	{
		if (ServerInstance->Time() > reset)
		{
			counter = 1;
			reset = ServerInstance->Time() + secs;
		}
		else
			counter++;
	}

	bool shouldlock() const
	{
		return (counter >= this->joins);
	}

	void clear()
	{
		counter = 0;
	}

	bool islocked()
	{
		if (ServerInstance->Time() > unlocktime)
			unlocktime = 0;

		return (unlocktime != 0);
	}

	void lock()
	{
		unlocktime = ServerInstance->Time() + duration;
	}

	bool operator==(const joinfloodsettings& other) const
	{
		return ((this->secs == other.secs) && (this->joins == other.joins));
	}
};

/** Handles channel mode +j
 */
class JoinFlood final
	: public ParamMode<JoinFlood, SimpleExtItem<joinfloodsettings>>
{
public:
	JoinFlood(Module* Creator)
		: ParamMode<JoinFlood, SimpleExtItem<joinfloodsettings>>(Creator, "joinflood", 'j')
	{
		syntax = "<joins>:<seconds>";
	}

	bool OnSet(User* source, Channel* channel, std::string& parameter) override
	{
		std::string::size_type colon = parameter.find(':');
		if ((colon == std::string::npos) || (parameter.find('-') != std::string::npos))
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return false;
		}

		/* Set up the flood parameters for this channel */
		unsigned int njoins = ConvToNum<unsigned int>(parameter.substr(0, colon));
		unsigned int nsecs = ConvToNum<unsigned int>(parameter.substr(colon+1));
		if ((njoins<1) || (nsecs<1))
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return false;
		}

		ext.SetFwd(channel, nsecs, njoins);
		return true;
	}

	void SerializeParam(Channel* chan, const joinfloodsettings* jfs, std::string& out)
	{
		out.append(ConvToStr(jfs->joins)).push_back(':');
		out.append(ConvToStr(jfs->secs));
	}
};

class ModuleJoinFlood final
	: public Module
	, public ServerProtocol::LinkEventListener
{
private:
	JoinFlood jf;
	time_t ignoreuntil = 0;
	unsigned long bootwait;
	unsigned long splitwait;
	ModeHandler::Rank notifyrank;

public:
	ModuleJoinFlood()
		: Module(VF_VENDOR, "Adds channel mode j (joinflood) which helps protect against spammers which mass-join channels.")
		, ServerProtocol::LinkEventListener(this)
		, jf(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("joinflood");
		duration = static_cast<unsigned int>(tag->getDuration("duration", 60, 10, 600));
		bootwait = tag->getDuration("bootwait", 30);
		splitwait = tag->getDuration("splitwait", 30);
		notifyrank = tag->getNum<ModeHandler::Rank>("notifyrank", 0);

		if (status.initial)
			ignoreuntil = ServerInstance->startup_time + bootwait;
	}

	void OnServerSplit(const Server* server, bool error) override
	{
		if (splitwait)
			ignoreuntil = std::max<time_t>(ignoreuntil, ServerInstance->Time() + splitwait);
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
	{
		if (!override && chan)
		{
			joinfloodsettings* f = jf.ext.Get(chan);
			if (f && f->islocked())
			{
				user->WriteNumeric(ERR_UNAVAILRESOURCE, chan->name, FMT::format("This channel is temporarily unavailable (+{} is set). Please try again later.",
					jf.GetModeChar()));
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& excepts) override
	{
		/* We arent interested in JOIN events caused by a network burst */
		if (sync || ignoreuntil > ServerInstance->Time())
			return;

		joinfloodsettings* f = jf.ext.Get(memb->chan);

		/* But all others are OK */
		if ((f) && (!f->islocked()))
		{
			f->addjoin();
			if (f->shouldlock())
			{
				f->clear();
				f->lock();

				PrefixMode* pm = ServerInstance->Modes.FindNearestPrefixMode(notifyrank);
				memb->chan->WriteNotice(FMT::format("This channel has been closed to new users for {} seconds because there have been more than {} joins in {} seconds.",
					duration, f->joins, f->secs), pm ? pm->GetPrefix() : 0);
			}
		}
	}
};

MODULE_INIT(ModuleJoinFlood)
