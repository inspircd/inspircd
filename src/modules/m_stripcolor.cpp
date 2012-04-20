/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2004, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDesc: Provides channel +S mode (strip ansi color) */

/** Handles channel mode +S
 */
class ChannelStripColor : public SimpleChannelModeHandler
{
 public:
	ChannelStripColor(Module* Creator) : SimpleChannelModeHandler(Creator, "stripcolor", 'S') { }
};

/** Handles user mode +S
 */
class UserStripColor : public SimpleUserModeHandler
{
 public:
	UserStripColor(Module* Creator) : SimpleUserModeHandler(Creator, "u_stripcolor", 'S') { }
};


class ModuleStripColor : public Module
{
	bool AllowChanOps;
	ChannelStripColor csc;
	UserStripColor usc;

 public:
	ModuleStripColor() : csc(this), usc(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(usc);
		ServerInstance->Modules->AddService(csc);
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ~ModuleStripColor()
	{
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('S');
	}

	virtual void ReplaceLine(std::string &sentence)
	{
		/* refactor this completely due to SQUIT bug since the old code would strip last char and replace with \0 --peavey */
		int seq = 0;
		std::string::iterator i,safei;
 		for (i = sentence.begin(); i != sentence.end();)
		{
			if ((*i == 3))
				seq = 1;
			else if (seq && (( ((*i >= '0') && (*i <= '9')) || (*i == ',') ) ))
			{
				seq++;
				if ( (seq <= 4) && (*i == ',') )
					seq = 1;
				else if (seq > 3)
					seq = 0;
			}
			else
				seq = 0;

			if (seq || ((*i == 2) || (*i == 15) || (*i == 22) || (*i == 21) || (*i == 31)))
			{
				if (i != sentence.begin())
				{
					safei = i;
					--i;
					sentence.erase(safei);
					++i;
				}
				else
				{
					sentence.erase(i);
					i = sentence.begin();
				}
			}
			else
				++i;
		}
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		bool active = false;
		if (target_type == TYPE_USER)
		{
			User* t = (User*)dest;
			active = t->IsModeSet('S');
		}
		else if (target_type == TYPE_CHANNEL)
		{
			Channel* t = (Channel*)dest;
			ModResult res = ServerInstance->OnCheckExemption(user,t,"stripcolor");

			if (res == MOD_RES_ALLOW)
				return MOD_RES_PASSTHRU;

			active = !t->GetExtBanStatus(user, 'S').check(!t->IsModeSet('S'));
		}

		if (active)
		{
			this->ReplaceLine(text);
		}

		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual Version GetVersion()
	{
		return Version("Provides channel +S mode (strip ansi color)", VF_VENDOR);
	}

};

MODULE_INIT(ModuleStripColor)
