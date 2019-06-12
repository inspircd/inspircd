/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2005 Craig McLure <craig@chatspike.net>
 *   Copyright (C) 2005 Craig Edwards <craigedwards@brainbox.cc>
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
#include "listmode.h"
#include "modules/exemption.h"

/** Handles channel mode +g
 */
class ChanFilter : public ListModeBase
{
 public:
	unsigned long maxlen;

	ChanFilter(Module* Creator)
		: ListModeBase(Creator, "filter", 'g', "End of channel spamfilter list", 941, 940, false)
	{
		syntax = "<pattern>";
	}

	bool ValidateParam(User* user, Channel* chan, std::string& word) CXX11_OVERRIDE
	{
		if (word.length() > maxlen)
		{
			user->WriteNumeric(Numerics::InvalidModeParameter(chan, this, word, "Word is too long for the spamfilter list."));
			return false;
		}

		return true;
	}
};

class ModuleChanFilter : public Module
{
	CheckExemption::EventProvider exemptionprov;
	ChanFilter cf;
	bool hidemask;
	bool notifyuser;

 public:

	ModuleChanFilter()
		: exemptionprov(this)
		, cf(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("chanfilter");
		hidemask = tag->getBool("hidemask");
		cf.maxlen = tag->getUInt("maxlen", 35, 10, 100);
		notifyuser = tag->getBool("notifyuser", true);
		cf.DoRehash();
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		if (target.type != MessageTarget::TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		Channel* chan = target.Get<Channel>();
		ModResult res = CheckExemption::Call(exemptionprov, user, chan, "filter");

		if (!IS_LOCAL(user) || res == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		ListModeBase::ModeList* list = cf.GetList(chan);

		if (list)
		{
			for (ListModeBase::ModeList::iterator i = list->begin(); i != list->end(); i++)
			{
				if (InspIRCd::Match(details.text, i->mask))
				{
					if (!notifyuser)
					{
						details.echo_original = true;
						return MOD_RES_DENY;
					}

					if (hidemask)
						user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (your message contained a censored word)");
					else
						user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (your message contained a censored word: " + i->mask + ")");
					return MOD_RES_DENY;
				}
			}
		}

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		// We don't send any link data if the length is 35 for compatibility with the 2.0 branch.
		std::string maxfilterlen;
		if (cf.maxlen != 35)
			maxfilterlen.assign(ConvToStr(cf.maxlen));

		return Version("Provides channel-specific censor lists (like mode +G but varies from channel to channel)", VF_VENDOR, maxfilterlen);
	}
};

MODULE_INIT(ModuleChanFilter)
