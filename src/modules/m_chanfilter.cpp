/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2018 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2008-2010 Craig Edwards <brain@inspircd.org>
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

	ChanFilter::ListItem* Match(User* user, Channel* chan, const std::string& text)
	{
		ModResult res = CheckExemption::Call(exemptionprov, user, chan, "filter");
		if (!IS_LOCAL(user) || res == MOD_RES_ALLOW)
			return NULL;

		ListModeBase::ModeList* list = cf.GetList(chan);
		if (!list)
			return NULL;

		for (ListModeBase::ModeList::iterator i = list->begin(); i != list->end(); i++)
		{
			if (InspIRCd::Match(text, i->mask))
				return &*i;
		}

		return NULL;
	}

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
		cf.maxlen = tag->getUInt("maxlen", 35, 10, ModeParser::MODE_PARAM_MAX);
		notifyuser = tag->getBool("notifyuser", true);
		cf.DoRehash();
	}

	void OnUserPart(Membership* memb, std::string& partmessage, CUList& except_list) CXX11_OVERRIDE
	{
		if (!memb)
			return;

		User* user = memb->user;
		Channel* chan = memb->chan;
		ChanFilter::ListItem* match = Match(user, chan, partmessage);
		if (!match)
			return;

		// Match() checks the user is local, we can assume from here
		LocalUser* luser = IS_LOCAL(user);

		std::string oldreason(partmessage);
		partmessage = "Reason filtered";
		if (!notifyuser)
		{
			// Send fake part
			ClientProtocol::Messages::Part partmsg(memb, oldreason);
			ClientProtocol::Event ev(ServerInstance->GetRFCEvents().part, partmsg);
			luser->Send(ev);

			// Don't send the user the changed message
			except_list.insert(user);
			return;
		}

		if (hidemask)
			user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (your part message contained a censored word)");
		else
			user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (your part message contained a censored word: " + match->mask + ")");
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		if (target.type != MessageTarget::TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		Channel* chan = target.Get<Channel>();
		ChanFilter::ListItem* match = Match(user, chan, details.text);
		if (match)
		{
			if (!notifyuser)
			{
				details.echo_original = true;
				return MOD_RES_DENY;
			}

			if (hidemask)
				user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (your message contained a censored word)");
			else
				user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (your message contained a censored word: " + match->mask + ")");

			return MOD_RES_DENY;
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
