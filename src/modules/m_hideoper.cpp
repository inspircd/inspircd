/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014 Adam <Adam@anope.org>
 *   Copyright (C) 2013, 2015, 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Geoff Bricker <geoff.bricker@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2010 Craig Edwards <brain@inspircd.org>
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
#include "modules/stats.h"
#include "modules/who.h"
#include "modules/whois.h"

/** Handles user mode +H
 */
class HideOper : public SimpleUserModeHandler
{
 public:
	size_t opercount;

	HideOper(Module* Creator) : SimpleUserModeHandler(Creator, "hideoper", 'H')
		, opercount(0)
	{
		oper = true;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string& parameter, bool adding) CXX11_OVERRIDE
	{
		if (SimpleUserModeHandler::OnModeChange(source, dest, channel, parameter, adding) == MODEACTION_DENY)
			return MODEACTION_DENY;

		if (adding)
			opercount++;
		else
			opercount--;

		return MODEACTION_ALLOW;
	}
};

class ModuleHideOper
	: public Module
	, public Stats::EventListener
	, public Who::EventListener
	, public Whois::LineEventListener
{
 private:
	HideOper hm;
	bool active;

 public:
	ModuleHideOper()
		: Stats::EventListener(this)
		, Who::EventListener(this)
		, Whois::LineEventListener(this)
		, hm(this)
		, active(false)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for hiding oper status with user mode +H", VF_VENDOR);
	}

	void OnUserQuit(User* user, const std::string&, const std::string&) CXX11_OVERRIDE
	{
		if (user->IsModeSet(hm))
			hm.opercount--;
	}

	ModResult OnNumeric(User* user, const Numeric::Numeric& numeric) CXX11_OVERRIDE
	{
		if (numeric.GetNumeric() != RPL_LUSEROP || active || user->HasPrivPermission("users/auspex"))
			return MOD_RES_PASSTHRU;

		// If there are no visible operators then we shouldn't send the numeric.
		size_t opercount = ServerInstance->Users->all_opers.size() - hm.opercount;
		if (opercount)
		{
			active = true;
			user->WriteNumeric(RPL_LUSEROP, opercount, "operator(s) online");
			active = false;
		}
		return MOD_RES_DENY;
	}

	ModResult OnWhoisLine(Whois::Context& whois, Numeric::Numeric& numeric) CXX11_OVERRIDE
	{
		/* Dont display numeric 313 (RPL_WHOISOPER) if they have +H set and the
		 * person doing the WHOIS is not an oper
		 */
		if (numeric.GetNumeric() != 313)
			return MOD_RES_PASSTHRU;

		if (!whois.GetTarget()->IsModeSet(hm))
			return MOD_RES_PASSTHRU;

		if (!whois.GetSource()->HasPrivPermission("users/auspex"))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}

	ModResult OnWhoLine(const Who::Request& request, LocalUser* source, User* user, Membership* memb, Numeric::Numeric& numeric) CXX11_OVERRIDE
	{
		if (user->IsModeSet(hm) && !source->HasPrivPermission("users/auspex"))
		{
			// Hide the line completely if doing a "/who * o" query
			if (request.flags['o'])
				return MOD_RES_DENY;

			size_t flag_index;
			if (!request.GetFieldIndex('f', flag_index))
				return MOD_RES_PASSTHRU;

			// hide the "*" that marks the user as an oper from the /WHO line
			// #chan ident localhost insp22.test nick H@ :0 Attila
			if (numeric.GetParams().size() <= flag_index)
				return MOD_RES_PASSTHRU;

			std::string& param = numeric.GetParams()[flag_index];
			const std::string::size_type pos = param.find('*');
			if (pos != std::string::npos)
				param.erase(pos, 1);
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE
	{
		if (stats.GetSymbol() != 'P')
			return MOD_RES_PASSTHRU;

		unsigned int count = 0;
		const UserManager::OperList& opers = ServerInstance->Users->all_opers;
		for (UserManager::OperList::const_iterator i = opers.begin(); i != opers.end(); ++i)
		{
			User* oper = *i;
			if (!oper->server->IsULine() && (stats.GetSource()->IsOper() || !oper->IsModeSet(hm)))
			{
				LocalUser* lu = IS_LOCAL(oper);
				stats.AddRow(249, oper->nick + " (" + oper->ident + "@" + oper->GetDisplayedHost() + ") Idle: " +
						(lu ? ConvToStr(ServerInstance->Time() - lu->idle_lastmsg) + " secs" : "unavailable"));
				count++;
			}
		}
		stats.AddRow(249, ConvToStr(count)+" OPER(s)");

		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleHideOper)
