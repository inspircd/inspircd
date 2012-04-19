/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
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


/* $ModDesc: Allows shorthand listmodes (+v user1,user2,user3 instead of +vvv user1 user2 user3) */

#include "inspircd.h"

class ModuleShorthandListmodes : public Module
{
 public:
	ModuleShorthandListmodes() {}

	void init()
	{
		ServerInstance->Modules->Attach(I_OnPreMode, this);
	}

	virtual ~ModuleShorthandListmodes()
	{
	}

	ModResult OnPreMode(User* u, Extensible*, irc::modestacker& ms)
	{
		if(!IS_LOCAL(u)) return MOD_RES_PASSTHRU;
		std::vector<irc::modechange> newsequence;
		std::string token;
		for(std::vector<irc::modechange>::iterator iter = ms.sequence.begin(); iter != ms.sequence.end(); ++iter)
		{
			ModeHandler* mh = ServerInstance->Modes->FindMode(iter->mode);
			if(mh->IsListMode() && mh->GetTranslateType() == TR_NICK && iter->value.find_first_of(',') != std::string::npos)
			{
				irc::commasepstream sep(iter->value);
				while(sep.GetToken(token))
					newsequence.push_back(irc::modechange(iter->mode, token, iter->adding));
			}
			else
				newsequence.push_back(*iter);
		}
		ms.sequence.swap(newsequence);
		return MOD_RES_PASSTHRU;
	}

	virtual Version GetVersion()
	{
		return Version("Allows shorthand listmodes (+v user1,user2,user3 instead of +vvv user1 user2 user3)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleShorthandListmodes)

