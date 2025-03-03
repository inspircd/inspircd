/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2016-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
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
#include "modules/exemption.h"
#include "numerichelper.h"
#include "timeutils.h"

// The number of seconds nickname changing will be blocked for.
static unsigned int duration;

/** Holds settings and state associated with channel mode +F
 */
class nickfloodsettings final
{
public:
	unsigned int secs;
	unsigned int nicks;
	time_t reset;
	time_t unlocktime = 0;
	unsigned int counter = 0;

	nickfloodsettings(unsigned int b, unsigned int c)
		: secs(b)
		, nicks(c)
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

	bool shouldlock() const
	{
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
class NickFlood final
	: public ParamMode<NickFlood, SimpleExtItem<nickfloodsettings>>
{
public:
	NickFlood(Module* Creator)
		: ParamMode<NickFlood, SimpleExtItem<nickfloodsettings>>(Creator, "nickflood", 'F')
	{
		syntax = "<nick-changes>:<seconds>";
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
		unsigned int nnicks = ConvToNum<unsigned int>(parameter.substr(0, colon));
		unsigned int nsecs = ConvToNum<unsigned int>(parameter.substr(colon+1));

		if ((nnicks<1) || (nsecs<1))
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return false;
		}

		ext.SetFwd(channel, nsecs, nnicks);
		return true;
	}

	void SerializeParam(Channel* chan, const nickfloodsettings* nfs, std::string& out)
	{
		out.append(ConvToStr(nfs->nicks)).push_back(':');
		out.append(ConvToStr(nfs->secs));
	}
};

class ModuleNickFlood final
	: public Module
{
private:
	CheckExemption::EventProvider exemptionprov;
	NickFlood nf;

public:
	ModuleNickFlood()
		: Module(VF_VENDOR, "Adds channel mode F (nickflood) which helps protect against spammers which mass-change nicknames.")
		, exemptionprov(this)
		, nf(this)
	{
	}

	void ReadConfig(ConfigStatus&) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("nickflood");
		duration = static_cast<unsigned int>(tag->getDuration("duration", 60, 10, 600));
	}

	ModResult OnUserPreNick(LocalUser* user, const std::string& newnick) override
	{
		for (const auto* memb : user->chans)
		{
			nickfloodsettings* f = nf.ext.Get(memb->chan);
			if (f)
			{
				ModResult res = exemptionprov.Check(user, memb->chan, "nickflood");
				if (res == MOD_RES_ALLOW)
					continue;

				if (f->islocked())
				{
					user->WriteNumeric(ERR_CANTCHANGENICK, INSP_FORMAT("{} has been locked for nick changes for {} because there have been more than {} nick changes in {}",
							memb->chan->name, Duration::ToHuman(duration), f->nicks, Duration::ToHuman(f->secs)));
					return MOD_RES_DENY;
				}

				if (f->shouldlock())
				{
					f->clear();
					f->lock();
					memb->chan->WriteNotice(INSP_FORMAT("No nick changes are allowed for {} because there have been more than {} nick changes in {}.",
						Duration::ToHuman(duration), f->nicks, Duration::ToHuman(f->secs)));
					return MOD_RES_DENY;
				}
			}
		}

		return MOD_RES_PASSTHRU;
	}

	/*
	 * XXX: HACK: We do the increment on the *POST* event here (instead of all together) because we have no way of knowing whether other modules would block a nickchange.
	 */
	void OnUserPostNick(User* user, const std::string& oldnick) override
	{
		if (isdigit(user->nick[0])) /* allow switches to UID */
			return;

		for (const auto* memb : user->chans)
		{
			nickfloodsettings* f = nf.ext.Get(memb->chan);
			if (f)
			{
				ModResult res = exemptionprov.Check(user, memb->chan, "nickflood");
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
};

MODULE_INIT(ModuleNickFlood)
