/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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
#include "modules/extban.h"
#include "modules/isupport.h"

enum
{
	// From RFC 2812.
	RPL_INVEXLIST = 346,
	RPL_ENDOFINVEXLIST = 347
};

class InviteException final
	: public ListModeBase
{
private:
	ExtBan::ManagerRef extbanmgr;

public:
	InviteException(Module* Creator)
		: ListModeBase(Creator, "invex", 'I', RPL_INVEXLIST, RPL_ENDOFINVEXLIST)
		, extbanmgr(Creator)
	{
		syntax = "<mask>";
	}

	bool CompareEntry(const std::string& entry, const std::string& value) const override
	{
		if (extbanmgr)
		{
			auto res = extbanmgr->CompareEntry(this, entry, value);
			if (res != ExtBan::Comparison::NOT_AN_EXTBAN)
				return res == ExtBan::Comparison::MATCH;
		}
		return irc::equals(entry, value);
	}

	bool ValidateParam(LocalUser* user, Channel* channel, std::string& parameter) override
	{
		if (!extbanmgr || !extbanmgr->Canonicalize(parameter))
			ModeParser::CleanMask(parameter);
		return true;
	}
};

class ModuleInviteException final
	: public Module
	, public ISupport::EventListener
{
private:
	bool invite_bypass_key;
	InviteException ie;

public:
	ModuleInviteException()
		: Module(VF_VENDOR, "Adds channel mode I (invex) which allows channel operators to exempt user masks from channel mode i (inviteonly).")
		, ISupport::EventListener(this)
		, ie(this)
	{
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["INVEX"] = ConvToStr(ie.GetModeChar());
	}

	ModResult OnCheckInvite(User* user, Channel* chan) override
	{
		ListModeBase::ModeList* list = ie.GetList(chan);
		if (list)
		{
			for (const auto& entry : *list)
			{
				if (chan->CheckBan(user, entry.mask))
				{
					return MOD_RES_ALLOW;
				}
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckKey(User* user, Channel* chan, const std::string& key) override
	{
		if (invite_bypass_key)
			return OnCheckInvite(user, chan);
		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus& status) override
	{
		ie.DoRehash();
		invite_bypass_key = ServerInstance->Config->ConfValue("inviteexception")->getBool("bypasskey", true);
	}
};

MODULE_INIT(ModuleInviteException)
