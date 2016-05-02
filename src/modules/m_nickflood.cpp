/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2009 Robin Burchell <robin+git@viroteck.net>
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

// The number of seconds nickname changing will be blocked for.
static unsigned int duration;

/** Holds settings and state associated with channel mode +F
 */
class nickfloodsettings
{
 public:
	unsigned int secs;
	unsigned int nicks;
	time_t reset;
	time_t unlocktime;
	unsigned int counter;

	nickfloodsettings(unsigned int b, unsigned int c)
		: secs(b), nicks(c), unlocktime(0), counter(0)
	{
		reset = ServerInstance->Time() + secs;
	}

	void addnick()
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
		/* XXX HACK: using counter + 1 here now to allow the counter to only be incremented
		 * on successful nick changes; this will be checked before the counter is
		 * incremented.
		 */
		return ((ServerInstance->Time() <= reset) && (counter == this->nicks));
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
};

/** Handles channel mode +F
 */
class NickFlood : public ParamMode<NickFlood, SimpleExtItem<nickfloodsettings> >
{
 public:
	NickFlood(Module* Creator)
		: ParamMode<NickFlood, SimpleExtItem<nickfloodsettings> >(Creator, "nickflood", 'F')
	{
	}

	ModeAction OnSet(User* source, Channel* channel, std::string& parameter)
	{
		std::string::size_type colon = parameter.find(':');
		if ((colon == std::string::npos) || (parameter.find('-') != std::string::npos))
		{
			source->WriteNumeric(608, channel->name, "Invalid flood parameter");
			return MODEACTION_DENY;
		}

		/* Set up the flood parameters for this channel */
		unsigned int nnicks = ConvToInt(parameter.substr(0, colon));
		unsigned int nsecs = ConvToInt(parameter.substr(colon+1));

		if ((nnicks<1) || (nsecs<1))
		{
			source->WriteNumeric(608, channel->name, "Invalid flood parameter");
			return MODEACTION_DENY;
		}

		ext.set(channel, new nickfloodsettings(nsecs, nnicks));
		return MODEACTION_ALLOW;
	}

	void SerializeParam(Channel* chan, const nickfloodsettings* nfs, std::string& out)
	{
		out.append(ConvToStr(nfs->nicks)).push_back(':');
		out.append(ConvToStr(nfs->secs));
	}
};

class ModuleNickFlood : public Module
{
	NickFlood nf;

 public:
	ModuleNickFlood()
		: nf(this)
	{
	}

	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("nickflood");
		duration = tag->getDuration("duration", 60, 10, 600);
	}

	ModResult OnUserPreNick(LocalUser* user, const std::string& newnick) CXX11_OVERRIDE
	{
		for (User::ChanList::iterator i = user->chans.begin(); i != user->chans.end(); i++)
		{
			Channel* channel = (*i)->chan;
			ModResult res;

			nickfloodsettings *f = nf.ext.get(channel);
			if (f)
			{
				res = ServerInstance->OnCheckExemption(user,channel,"nickflood");
				if (res == MOD_RES_ALLOW)
					continue;

				if (f->islocked())
				{
					user->WriteNumeric(ERR_CANTCHANGENICK, InspIRCd::Format("%s has been locked for nickchanges for %u seconds because there have been more than %u nick changes in %u seconds", channel->name.c_str(), duration, f->nicks, f->secs));
					return MOD_RES_DENY;
				}

				if (f->shouldlock())
				{
					f->clear();
					f->lock();
					channel->WriteNotice(InspIRCd::Format("No nick changes are allowed for %u seconds because there have been more than %u nick changes in %u seconds.", duration, f->nicks, f->secs));
					return MOD_RES_DENY;
				}
			}
		}

		return MOD_RES_PASSTHRU;
	}

	/*
	 * XXX: HACK: We do the increment on the *POST* event here (instead of all together) because we have no way of knowing whether other modules would block a nickchange.
	 */
	void OnUserPostNick(User* user, const std::string &oldnick) CXX11_OVERRIDE
	{
		if (isdigit(user->nick[0])) /* allow switches to UID */
			return;

		for (User::ChanList::iterator i = user->chans.begin(); i != user->chans.end(); ++i)
		{
			Channel* channel = (*i)->chan;
			ModResult res;

			nickfloodsettings *f = nf.ext.get(channel);
			if (f)
			{
				res = ServerInstance->OnCheckExemption(user,channel,"nickflood");
				if (res == MOD_RES_ALLOW)
					return;

				/* moved this here to avoid incrementing the counter for nick
				 * changes that are denied for some other reason (bans, +N, etc.)
				 * per bug #874.
				 */
				f->addnick();
			}
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Channel mode F - nick flood protection", VF_VENDOR);
	}
};

MODULE_INIT(ModuleNickFlood)
