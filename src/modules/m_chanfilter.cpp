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
	ChanFilter(Module* Creator) : ListModeBase(Creator, "filter", 'g', "End of channel spamfilter list", 941, 940, false, "chanfilter") { }

	bool ValidateParam(User* user, Channel* chan, std::string &word)
	{
		if (word.length() > 35)
		{
			user->WriteNumeric(935, chan->name, word, "%word is too long for censor list");
			return false;
		}

		return true;
	}

	void TellListTooLong(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(939, chan->name, word, "Channel spamfilter list is full");
	}

	void TellAlreadyOnList(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(937, chan->name, InspIRCd::Format("The word %s is already on the spamfilter list", word.c_str()));
	}

	void TellNotSet(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(938, chan->name, "No such spamfilter word is set");
	}
};

class ModuleChanFilter : public Module
{
	CheckExemption::EventProvider exemptionprov;
	ChanFilter cf;
	bool hidemask;

 public:

	ModuleChanFilter()
		: exemptionprov(this)
		, cf(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		hidemask = ServerInstance->Config->ConfValue("chanfilter")->getBool("hidemask");
		cf.DoRehash();
	}

	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list, MessageType msgtype) CXX11_OVERRIDE
	{
		if (target_type != TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		Channel* chan = static_cast<Channel*>(dest);
		ModResult res;
		FIRST_MOD_RESULT_CUSTOM(exemptionprov, CheckExemption::EventListener, OnCheckExemption, res, (user, chan, "filter"));

		if (!IS_LOCAL(user) || res == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		ListModeBase::ModeList* list = cf.GetList(chan);

		if (list)
		{
			for (ListModeBase::ModeList::iterator i = list->begin(); i != list->end(); i++)
			{
				if (InspIRCd::Match(text, i->mask))
				{
					if (hidemask)
						user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (your message contained a censored word)");
					else
						user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, i->mask, "Cannot send to channel (your message contained a censored word)");
					return MOD_RES_DENY;
				}
			}
		}

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel-specific censor lists (like mode +G but varies from channel to channel)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleChanFilter)
