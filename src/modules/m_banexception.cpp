/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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
	RPL_EXCEPTLIST = 348,
	RPL_ENDOFEXCEPTLIST = 349
};

class BanException : public ListModeBase
{
 public:
	BanException(Module* Creator)
		: ListModeBase(Creator, "banexception", 'e', "End of Channel Exception List", RPL_EXCEPTLIST, RPL_ENDOFEXCEPTLIST, true)
	{
		syntax = "<mask>";
	}
};

class ModuleBanException
	: public Module
	, public ExtBan::EventListener
	, public ISupport::EventListener
{
 private:
	BanException be;

 public:
	ModuleBanException()
		: Module(VF_VENDOR, "Adds channel mode e (banexception) which allows channel operators to exempt user masks from the b (ban) channel mode.")
		, ExtBan::EventListener(this)
		, ISupport::EventListener(this)
		, be(this)
	{
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["EXCEPTS"] = ConvToStr(be.GetModeChar());
	}

	ModResult OnExtBanCheck(User* user, Channel* chan, ExtBan::Base* extban) override
	{
		ListModeBase::ModeList* list = be.GetList(chan);
		if (!list)
			return MOD_RES_PASSTHRU;

		for (const ListModeBase::ListItem& ban : *list)
		{
			bool inverted;
			std::string name, value;
			if (!ExtBan::Parse(ban.mask, name, value, inverted))
				continue;

			if (name.size() == 1)
			{
				// It is an extban but not this extban.
				if (name[0] != extban->GetLetter())
					continue;
			}
			else
			{
				// It is an extban but not this extban.
				if (!irc::equals(name, extban->GetName()))
					continue;
			}

			return extban->IsMatch(user, chan, value) != inverted ? MOD_RES_ALLOW : MOD_RES_PASSTHRU;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckChannelBan(User* user, Channel* chan) override
	{
		ListModeBase::ModeList* list = be.GetList(chan);
		if (!list)
		{
			// No list, proceed normally
			return MOD_RES_PASSTHRU;
		}

		for (ListModeBase::ModeList::iterator it = list->begin(); it != list->end(); it++)
		{
			if (chan->CheckBan(user, it->mask))
			{
				// They match an entry on the list, so let them in.
				return MOD_RES_ALLOW;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus& status) override
	{
		be.DoRehash();
	}
};

MODULE_INIT(ModuleBanException)
