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

/* $ModDesc: Provides support for hiding oper status with user mode +H */

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

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string& parameter, bool adding)
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

class ModuleHideOper : public Module
{
	HideOper hm;
	bool active;
 public:
	ModuleHideOper()
		: hm(this)
		, active(false)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(hm);
		Implementation eventlist[] = { I_OnWhoisLine, I_OnSendWhoLine, I_OnStats, I_OnNumeric, I_OnUserQuit };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}


	virtual ~ModuleHideOper()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for hiding oper status with user mode +H", VF_VENDOR);
	}

	void OnUserQuit(User* user, const std::string&, const std::string&)
	{
		if (user->IsModeSet('H'))
			hm.opercount--;
	}

	ModResult OnNumeric(User* user, unsigned int numeric, const std::string& text)
	{
		if (numeric != 252 || active || user->HasPrivPermission("users/auspex"))
			return MOD_RES_PASSTHRU;

		// If there are no visible operators then we shouldn't send the numeric.
		size_t opercount = ServerInstance->Users->all_opers.size() - hm.opercount;
		if (opercount)
		{
			active = true;
			user->WriteNumeric(252, "%s %lu :operator(s) online", user->nick.c_str(),  static_cast<unsigned long>(opercount));
			active = false;
		}
		return MOD_RES_DENY;
	}

	ModResult OnWhoisLine(User* user, User* dest, int &numeric, std::string &text)
	{
		/* Dont display numeric 313 (RPL_WHOISOPER) if they have +H set and the
		 * person doing the WHOIS is not an oper
		 */
		if (numeric != 313)
			return MOD_RES_PASSTHRU;

		if (!dest->IsModeSet('H'))
			return MOD_RES_PASSTHRU;

		if (!user->HasPrivPermission("users/auspex"))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}

	void OnSendWhoLine(User* source, const std::vector<std::string>& params, User* user, std::string& line)
	{
		if (user->IsModeSet('H') && !source->HasPrivPermission("users/auspex"))
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

	ModResult OnStats(char symbol, User* user, string_list &results)
	{
		if (symbol != 'P')
			return MOD_RES_PASSTHRU;

		unsigned int count = 0;
		for (std::list<User*>::const_iterator i = ServerInstance->Users->all_opers.begin(); i != ServerInstance->Users->all_opers.end(); ++i)
		{
			User* oper = *i;
			if (!ServerInstance->ULine(oper->server) && (IS_OPER(user) || !oper->IsModeSet('H')))
			{
				results.push_back(ServerInstance->Config->ServerName+" 249 " + user->nick + " :" + oper->nick + " (" + oper->ident + "@" + oper->dhost + ") Idle: " +
						(IS_LOCAL(oper) ? ConvToStr(ServerInstance->Time() - oper->idle_lastmsg) + " secs" : "unavailable"));
				count++;
			}
		}
		results.push_back(ServerInstance->Config->ServerName+" 249 "+user->nick+" :"+ConvToStr(count)+" OPER(s)");

		return MOD_RES_DENY;
	}
};


MODULE_INIT(ModuleHideOper)
