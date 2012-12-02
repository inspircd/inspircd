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

/* $ModDesc: Provides channel mode +F (nick flood protection) */

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
		unlocktime = ServerInstance->Time() + 60;
	}
};

/** Handles channel mode +F
 */
class NickFlood : public ModeHandler
{
 public:
	SimpleExtItem<nickfloodsettings> ext;
	NickFlood(Module* Creator) : ModeHandler(Creator, "nickflood", 'F', PARAM_SETONLY, MODETYPE_CHANNEL),
		ext("nickflood", Creator) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			std::string::size_type colon = parameter.find(':');
			if ((colon == std::string::npos) || (parameter.find('-') != std::string::npos))
			{
				source->WriteNumeric(608, "%s %s :Invalid flood parameter",source->nick.c_str(),channel->name.c_str());
				return MODEACTION_DENY;
			}

			/* Set up the flood parameters for this channel */
			unsigned int nnicks = ConvToInt(parameter.substr(0, colon));
			unsigned int nsecs = ConvToInt(parameter.substr(colon+1));

			if ((nnicks<1) || (nsecs<1))
			{
				source->WriteNumeric(608, "%s %s :Invalid flood parameter",source->nick.c_str(),channel->name.c_str());
				return MODEACTION_DENY;
			}

			nickfloodsettings* f = ext.get(channel);
			if ((f) && (nnicks == f->nicks) && (nsecs == f->secs))
				// mode params match
				return MODEACTION_DENY;

			ext.set(channel, new nickfloodsettings(nsecs, nnicks));
			parameter = ConvToStr(nnicks) + ":" + ConvToStr(nsecs);
			channel->SetModeParam('F', parameter);
			return MODEACTION_ALLOW;
		}
		else
		{
			if (!channel->IsModeSet('F'))
				return MODEACTION_DENY;

			ext.unset(channel);
			channel->SetModeParam('F', "");
			return MODEACTION_ALLOW;
		}
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

	void init()
	{
		ServerInstance->Modules->AddService(nf);
		ServerInstance->Modules->AddService(nf.ext);
		Implementation eventlist[] = { I_OnUserPreNick, I_OnUserPostNick };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnUserPreNick(User* user, const std::string &newnick)
	{
		if (ServerInstance->NICKForced.get(user)) /* Allow forced nick changes */
			return MOD_RES_PASSTHRU;

		for (UCListIter i = user->chans.begin(); i != user->chans.end(); i++)
		{
			Channel *channel = *i;
			ModResult res;

			nickfloodsettings *f = nf.ext.get(channel);
			if (f)
			{
				res = ServerInstance->OnCheckExemption(user,channel,"nickflood");
				if (res == MOD_RES_ALLOW)
					continue;

				if (f->islocked())
				{
					user->WriteNumeric(447, "%s :%s has been locked for nickchanges for 60 seconds because there have been more than %u nick changes in %u seconds", user->nick.c_str(), channel->name.c_str(), f->nicks, f->secs);
					return MOD_RES_DENY;
				}

				if (f->shouldlock())
				{
					f->clear();
					f->lock();
					channel->WriteChannelWithServ((char*)ServerInstance->Config->ServerName.c_str(), "NOTICE %s :No nick changes are allowed for 60 seconds because there have been more than %u nick changes in %u seconds.", channel->name.c_str(), f->nicks, f->secs);
					return MOD_RES_DENY;
				}
			}
		}

		return MOD_RES_PASSTHRU;
	}

	/*
	 * XXX: HACK: We do the increment on the *POST* event here (instead of all together) because we have no way of knowing whether other modules would block a nickchange.
	 */
	void OnUserPostNick(User* user, const std::string &oldnick)
	{
		if (isdigit(user->nick[0])) /* allow switches to UID */
			return;

		for (UCListIter i = user->chans.begin(); i != user->chans.end(); ++i)
		{
			Channel *channel = *i;
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

	~ModuleNickFlood()
	{
	}

	Version GetVersion()
	{
		return Version("Channel mode F - nick flood protection", VF_VENDOR);
	}
};

MODULE_INIT(ModuleNickFlood)
