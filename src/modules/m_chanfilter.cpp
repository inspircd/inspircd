/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2018 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2008-2009 Craig Edwards <brain@inspircd.org>
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
#include "clientprotocolmsg.h"
#include "listmode.h"
#include "modules/exemption.h"
#include "numerichelper.h"

enum
{
	// InspIRCd-specific.
	RPL_ENDOFSPAMFILTER = 940,
	RPL_SPAMFILTER = 941
};

class ChanFilter final
	: public ListModeBase
{
public:
	unsigned long maxlen;

	ChanFilter(Module* Creator)
		: ListModeBase(Creator, "filter", 'g', RPL_SPAMFILTER, RPL_ENDOFSPAMFILTER)
	{
		syntax = "<pattern>";
	}

	bool ValidateParam(LocalUser* user, Channel* chan, std::string& parameter) override
	{
		// We only enforce the length restriction against local users to avoid causing a desync.
		if (parameter.length() > maxlen)
		{
			user->WriteNumeric(Numerics::InvalidModeParameter(chan, this, parameter, "Entry is too long for the spamfilter list."));
			return false;
		}

		return true;
	}
};

class ModuleChanFilter final
	: public Module
{
	CheckExemption::EventProvider exemptionprov;
	ChanFilter cf;
	bool hidemask;
	bool notifyuser;

	const ChanFilter::ListItem* Match(User* user, Channel* chan, const std::string& text)
	{
		if (!IS_LOCAL(user))
			return nullptr; // We don't handle remote users.

		if (user->HasPrivPermission("channels/ignore-chanfilter"))
			return nullptr; // The source is an exempt server operator.

		if (exemptionprov.Check(user, chan, "filter") == MOD_RES_ALLOW)
			return nullptr; // The source matches an exemptchanops entry.

		ListModeBase::ModeList* list = cf.GetList(chan);
		if (!list)
			return nullptr;

		for (const auto& entry : *list)
		{
			if (InspIRCd::Match(text, entry.mask))
				return &entry;
		}

		return nullptr;
	}

public:

	ModuleChanFilter()
		: Module(VF_VENDOR, "Adds channel mode g (filter) which allows channel operators to define glob patterns for inappropriate phrases that are not allowed to be used in the channel.")
		, exemptionprov(this)
		, cf(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("chanfilter");
		hidemask = tag->getBool("hidemask");
		cf.maxlen = tag->getNum<unsigned long>("maxlen", 35, 10, ModeParser::MODE_PARAM_MAX);
		notifyuser = tag->getBool("notifyuser", true);
		cf.DoRehash();
	}

	void OnUserPart(Membership* memb, std::string& partmessage, CUList& except_list) override
	{
		if (!memb)
			return;

		User* user = memb->user;
		Channel* chan = memb->chan;
		const ChanFilter::ListItem* match = Match(user, chan, partmessage);
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
			user->WriteNumeric(Numerics::CannotSendTo(chan, "Your part message contained a banned phrase and was blocked."));
		else
		{
			user->WriteNumeric(Numerics::CannotSendTo(chan, INSP_FORMAT("Your part message contained a banned phrase ({}) and was blocked.",
				match->mask)));
		}
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		if (target.type != MessageTarget::TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		Channel* chan = target.Get<Channel>();
		const ChanFilter::ListItem* match = Match(user, chan, details.text);
		if (match)
		{
			if (!notifyuser)
			{
				details.echo_original = true;
				return MOD_RES_DENY;
			}

			if (hidemask)
				user->WriteNumeric(Numerics::CannotSendTo(chan, "Your message to this channel contained a banned phrase and was blocked."));
			else
			{
				user->WriteNumeric(Numerics::CannotSendTo(chan, INSP_FORMAT("Your message to this channel contained a banned phrase ({}) and was blocked.",
					match->mask)));
			}

			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void GetLinkData(LinkData& data, std::string& compatdata) override
	{
		data["max-length"] = ConvToStr(cf.maxlen);

		// We don't send any link data if the length is 35 for compatibility with v3 and earlier..
		if (cf.maxlen != 35)
			compatdata = ConvToStr(cf.maxlen);
	}
};

MODULE_INIT(ModuleChanFilter)
