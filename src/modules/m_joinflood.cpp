/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
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
		unlocktime = ServerInstance->Time() + 60;
	}

	bool operator==(const joinfloodsettings& other) const
	{
		return ((this->secs == other.secs) && (this->joins == other.joins));
	}
};

/** Handles channel mode +j
 */
class JoinFlood : public ModeHandler
{
 public:
	SimpleExtItem<joinfloodsettings> ext;
	JoinFlood(Module* Creator) : ModeHandler(Creator, "joinflood", 'j', PARAM_SETONLY, MODETYPE_CHANNEL),
		ext("joinflood", Creator) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			std::string::size_type colon = parameter.find(':');
			if ((colon == std::string::npos) || (parameter.find('-') != std::string::npos))
			{
				source->WriteNumeric(608, "%s :Invalid flood parameter",channel->name.c_str());
				return MODEACTION_DENY;
			}

			/* Set up the flood parameters for this channel */
			unsigned int njoins = ConvToInt(parameter.substr(0, colon));
			unsigned int nsecs = ConvToInt(parameter.substr(colon+1));
			if ((njoins<1) || (nsecs<1))
			{
				source->WriteNumeric(608, "%s :Invalid flood parameter",channel->name.c_str());
				return MODEACTION_DENY;
			}

			joinfloodsettings jfs(nsecs, njoins);
			joinfloodsettings* f = ext.get(channel);
			if ((f) && (*f == jfs))
				// mode params match
				return MODEACTION_DENY;

			ext.set(channel, jfs);
			parameter = ConvToStr(njoins) + ":" + ConvToStr(nsecs);
			return MODEACTION_ALLOW;
		}
		else
		{
			if (!channel->IsModeSet(this))
				return MODEACTION_DENY;

			ext.unset(channel);
			return MODEACTION_ALLOW;
		}
		return MODEACTION_DENY;
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

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		if (chan)
		{
			joinfloodsettings *f = jf.ext.get(chan);
			if (f && f->islocked())
			{
				user->WriteNumeric(609, "%s :This channel is temporarily unavailable (+j). Please try again later.",chan->name.c_str());
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
				memb->chan->WriteChannelWithServ((char*)ServerInstance->Config->ServerName.c_str(), "NOTICE %s :This channel has been closed to new users for 60 seconds because there have been more than %d joins in %d seconds.", memb->chan->name.c_str(), f->joins, f->secs);
			}
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel mode +j (join flood protection)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleJoinFlood)
