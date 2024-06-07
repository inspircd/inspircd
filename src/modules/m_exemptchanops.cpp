/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "numerichelper.h"

enum
{
	RPL_ENDOFEXEMPTIONLIST = 953,
	RPL_EXEMPTIONLIST = 954
};

class ExemptChanOps final
	: public ListModeBase
{
public:
	ExemptChanOps(Module* Creator)
		: ListModeBase(Creator, "exemptchanops", 'X', RPL_EXEMPTIONLIST, RPL_ENDOFEXEMPTIONLIST)
	{
		syntax = "<restriction>:<prefix>";
	}

	static PrefixMode* FindMode(const std::string& pmode)
	{
		if (pmode.length() == 1)
		{
			PrefixMode* pm = ServerInstance->Modes.FindPrefixMode(pmode[0]);
			if (!pm)
				pm = ServerInstance->Modes.FindPrefix(pmode[0]);
			return pm;
		}

		ModeHandler* mh = ServerInstance->Modes.FindMode(pmode, MODETYPE_CHANNEL);
		return mh ? mh->IsPrefixMode() : nullptr;
	}

	static bool ParseEntry(const std::string& entry, std::string& restriction, std::string& prefix)
	{
		// The entry must be in the format <restriction>:<prefix>.
		std::string::size_type colon = entry.find(':');
		if (colon == std::string::npos || colon == entry.length()-1)
			return false;

		restriction.assign(entry, 0, colon);
		prefix.assign(entry, colon + 1, std::string::npos);
		return true;
	}

	ModResult AccessCheck(User* source, Channel* channel, Modes::Change& change) override
	{
		std::string restriction;
		std::string prefix;
		if (!ParseEntry(change.param, restriction, prefix))
			return MOD_RES_PASSTHRU;

		PrefixMode* pm = FindMode(prefix);
		if (!pm)
			return MOD_RES_PASSTHRU;

		if (channel->GetPrefixValue(source) >= pm->GetLevelRequired(change.adding))
			return MOD_RES_PASSTHRU;

		source->WriteNumeric(ERR_CHANOPRIVSNEEDED, channel->name, fmt::format("You must be able to {} mode {} ({}) to {} a restriction containing it",
			change.adding ? "set" : "unset", pm->GetModeChar(), pm->name, change.adding ? "add" : "remove"));
		return MOD_RES_DENY;
	}

	bool ValidateParam(LocalUser* user, Channel* chan, std::string& parameter) override
	{
		// We only enforce the format restriction against local users to avoid causing a desync.
		std::string restriction;
		std::string prefix;
		if (!ParseEntry(parameter, restriction, prefix))
		{
			user->WriteNumeric(Numerics::InvalidModeParameter(chan, this, parameter));
			return false;
		}

		// If there is a '-' in the restriction string ignore it and everything after it
		// to support "auditorium-vis" and "auditorium-see" in m_auditorium
		std::string::size_type dash = restriction.find('-');
		if (dash != std::string::npos)
			restriction.erase(dash);

		if (!ServerInstance->Modes.FindMode(restriction, MODETYPE_CHANNEL))
		{
			user->WriteNumeric(Numerics::InvalidModeParameter(chan, this, parameter, "Unknown restriction."));
			return false;
		}

		if (prefix != "*" && !FindMode(prefix))
		{
			user->WriteNumeric(Numerics::InvalidModeParameter(chan, this, parameter, "Unknown prefix mode."));
			return false;
		}

		return true;
	}
};

class ExemptHandler final
	: public CheckExemption::EventListener
{
public:
	ExemptChanOps ec;
	ExemptHandler(Module* me)
		: CheckExemption::EventListener(me)
		, ec(me)
	{
	}

	ModResult OnCheckExemption(User* user, Channel* chan, const std::string& restriction) override
	{
		ModeHandler::Rank mypfx = chan->GetPrefixValue(user);
		std::string minmode;

		ListModeBase::ModeList* list = ec.GetList(chan);

		if (list)
		{
			for (const auto& entry : *list)
			{
				std::string::size_type pos = entry.mask.find(':');
				if (pos == std::string::npos)
					continue;

				if (!entry.mask.compare(0, pos, restriction))
					minmode.assign(entry.mask, pos + 1, std::string::npos);
			}
		}

		PrefixMode* mh = ExemptChanOps::FindMode(minmode);
		if (mh && mypfx >= mh->GetPrefixRank())
			return MOD_RES_ALLOW;
		if (mh || minmode == "*")
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}
};

class ModuleExemptChanOps final
	: public Module
{
private:
	ExemptHandler eh;

public:
	ModuleExemptChanOps()
		: Module(VF_VENDOR, "Adds channel mode X (exemptchanops) which allows channel operators to grant exemptions to various channel-level restrictions.")
		, eh(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		eh.ec.DoRehash();
	}
};

MODULE_INIT(ModuleExemptChanOps)
