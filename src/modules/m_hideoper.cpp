/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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

/** Handles user mode +H
 */
class HideOper : public SimpleUserModeHandler
{
 public:
	HideOper(Module* Creator) : SimpleUserModeHandler(Creator, "hideoper", 'H')
	{
		oper = true;
	}
};

class ModuleHideOper : public Module, public Whois::LineEventListener
{
	HideOper hm;
 public:
	ModuleHideOper()
		: Whois::LineEventListener(this)
		, hm(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for hiding oper status with user mode +H", VF_VENDOR);
	}

	ModResult OnWhoisLine(Whois::Context& whois, unsigned int& numeric, std::string& text) CXX11_OVERRIDE
	{
		/* Dont display numeric 313 (RPL_WHOISOPER) if they have +H set and the
		 * person doing the WHOIS is not an oper
		 */
		if (numeric != 313)
			return MOD_RES_PASSTHRU;

		if (!whois.GetTarget()->IsModeSet(hm))
			return MOD_RES_PASSTHRU;

		if (!whois.GetSource()->HasPrivPermission("users/auspex"))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}

	void OnSendWhoLine(User* source, const std::vector<std::string>& params, User* user, Membership* memb, std::string& line) CXX11_OVERRIDE
	{
		if (user->IsModeSet(hm) && !source->HasPrivPermission("users/auspex"))
		{
			// hide the "*" that marks the user as an oper from the /WHO line
			std::string::size_type spcolon = line.find(" :");
			if (spcolon == std::string::npos)
				return; // Another module hid the user completely
			std::string::size_type sp = line.rfind(' ', spcolon-1);
			std::string::size_type pos = line.find('*', sp);
			if (pos != std::string::npos)
				line.erase(pos, 1);
			// hide the line completely if doing a "/who * o" query
			if (params.size() > 1 && params[1].find('o') != std::string::npos)
				line.clear();
		}
	}

	ModResult OnStats(char symbol, User* user, string_list& results) CXX11_OVERRIDE
	{
		if (symbol != 'P')
			return MOD_RES_PASSTHRU;

		unsigned int count = 0;
		const UserManager::OperList& opers = ServerInstance->Users->all_opers;
		for (UserManager::OperList::const_iterator i = opers.begin(); i != opers.end(); ++i)
		{
			User* oper = *i;
			if (!oper->server->IsULine() && (user->IsOper() || !oper->IsModeSet(hm)))
			{
				LocalUser* lu = IS_LOCAL(oper);
				results.push_back("249 " + user->nick + " :" + oper->nick + " (" + oper->ident + "@" + oper->dhost + ") Idle: " +
						(lu ? ConvToStr(ServerInstance->Time() - lu->idle_lastmsg) + " secs" : "unavailable"));
				count++;
			}
		}
		results.push_back("249 "+user->nick+" :"+ConvToStr(count)+" OPER(s)");

		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleHideOper)
