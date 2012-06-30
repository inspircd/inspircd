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
class HideOper : public ModeHandler
{
 public:
	HideOper(Module* Creator) : ModeHandler(Creator, "hideoper", 'H', PARAM_NONE, MODETYPE_USER)
	{
		oper = true;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('H'))
			{
				dest->SetMode('H',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('H'))
			{
				dest->SetMode('H',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleHideOper : public Module
{
	HideOper hm;
	unsigned int opers_to_show;
 public:
	ModuleHideOper()
		: hm(this), opers_to_show(0)
	{
		if (!ServerInstance->Modes->AddMode(&hm))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnWhoisLine, I_OnSendWhoLine, I_OnStatsLine };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}


	virtual ~ModuleHideOper()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for hiding oper status with user mode +H", VF_VENDOR);
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
			std::string::size_type pos = line.find("*");
			if (pos != std::string::npos)
				line.erase(pos, 1);
			// hide the line completely if doing a "/who * o" query
			if (params.size() > 1 && params[1].find('o') != std::string::npos)
				line.clear();
		}
	}

	void OnStatsLine(User* user, char symbol, std::string& line, bool core_generated)
	{
		if ((symbol != 'P') || (!core_generated) || (user->HasPrivPermission("users/auspex")))
			return;

		/**
		STATS P
		:insp20.test 249 20DAAAAAA :nick (ident@host.co.uk) Idle: 10 secs
		:insp20.test 249 20DAAAAAA :1 OPER(s)
		*/

		std::string::size_type a = line.find(':', 1);
		if (a == std::string::npos)
			return;

		a++;
		std::string::size_type b = line.find(' ', a);
		if (b == std::string::npos)
			return;

		if (line[line.size()-1] == ')')
		{
			// Last line, remove the original oper count and insert ours
			while (isdigit(line[a]))
				line.erase(a, 1);

			line.insert(a, ConvToStr(opers_to_show));
			opers_to_show = 0;
		}
		else
		{
			User* oper = ServerInstance->FindNick(line.substr(a, b-a));
			if ((oper) && (oper->IsModeSet('H')))
				// Oper with usermode +H, hide the line
				line.clear();
			else
				opers_to_show++;
		}
	}
};


MODULE_INIT(ModuleHideOper)
