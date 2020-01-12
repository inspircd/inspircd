/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2016-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007, 2010 Craig Edwards <brain@inspircd.org>
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

enum
{
	// From RFC 2182.
	ERR_UNAVAILRESOURCE = 437
};

// The number of seconds the channel will be closed for.
static unsigned int duration;

// Enable notice to channel when it is triggered
bool notify;

// Only notify to channel operators
bool notify_onlyops;

/** Holds settings and state associated with channel mode +j
 */
class joinfloodsettings
{
 public:
	unsigned int secs;
	unsigned int joins;
	time_t reset;
	time_t unlocktime;
	unsigned int counter;

	joinfloodsettings(unsigned int b, unsigned int c)
		: secs(b), joins(c), unlocktime(0), counter(0)
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

	bool shouldlock()
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
class JoinFlood : public ParamMode<JoinFlood, SimpleExtItem<joinfloodsettings> >
{
 public:
	JoinFlood(Module* Creator)
		: ParamMode<JoinFlood, SimpleExtItem<joinfloodsettings> >(Creator, "joinflood", 'j')
	{
		syntax = "<joins>:<seconds>";
	}

	ModeAction OnSet(User* source, Channel* channel, std::string& parameter) CXX11_OVERRIDE
	{
		std::string::size_type colon = parameter.find(':');
		if ((colon == std::string::npos) || (parameter.find('-') != std::string::npos))
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return MODEACTION_DENY;
		}

		/* Set up the flood parameters for this channel */
		unsigned int njoins = ConvToNum<unsigned int>(parameter.substr(0, colon));
		unsigned int nsecs = ConvToNum<unsigned int>(parameter.substr(colon+1));
		if ((njoins<1) || (nsecs<1))
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return MODEACTION_DENY;
		}

		ext.set(channel, new joinfloodsettings(nsecs, njoins));
		return MODEACTION_ALLOW;
	}

	void SerializeParam(Channel* chan, const joinfloodsettings* jfs, std::string& out)
	{
		out.append(ConvToStr(jfs->joins)).push_back(':');
		out.append(ConvToStr(jfs->secs));
	}
};

class ModuleJoinFlood : public Module
{
	JoinFlood jf;

 public:
	ModuleJoinFlood()
		: jf(this)
	{
	}

	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("joinflood");
		duration = tag->getDuration("duration", 60, 10, 600);
		notify = tag->getBool("notify", true);
		notify_onlyops = tag->getBool("notify_onlyops", false);
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		if (chan)
		{
			joinfloodsettings *f = jf.ext.get(chan);
			if (f && f->islocked())
			{
				user->WriteNumeric(ERR_UNAVAILRESOURCE, chan->name, "This channel is temporarily unavailable (+j is set). Please try again later.");
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& excepts) CXX11_OVERRIDE
	{
		/* We arent interested in JOIN events caused by a network burst */
		if (sync)
			return;

		joinfloodsettings *f = jf.ext.get(memb->chan);

		/* But all others are OK */
		if ((f) && (!f->islocked()))
		{
			f->addjoin();
			if (f->shouldlock())
			{
				f->clear();
				f->lock();
				if (notify) {
					std::string message = InspIRCd::Format("This channel has been closed to new users for %u seconds because there have been more than %d joins in %d seconds.", duration, f->joins, f->secs);
					ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->FakeClient, memb->chan, message, MSG_NOTICE);
					memb->chan->Write(ServerInstance->GetRFCEvents().privmsg, privmsg, notify_onlyops ? '@' : 0);
					ServerInstance->PI->SendMessage(memb->chan, notify_onlyops ? '@' : 0, message, MSG_NOTICE);
				}
			}
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel mode +j, join flood protection", VF_VENDOR);
	}
};

MODULE_INIT(ModuleJoinFlood)
