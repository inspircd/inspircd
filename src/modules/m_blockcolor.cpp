/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2004-2006, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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

/* $ModDesc: Provides channel mode +c to block color */

/** Handles the +c channel mode
 */
class BlockColor : public SimpleChannelModeHandler
{
 public:
	BlockColor(Module* Creator) : SimpleChannelModeHandler(Creator, "blockcolor", 'c') { }
};

class ModuleBlockColor : public Module
{
	BlockColor bc;
 public:

	ModuleBlockColor() : bc(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(bc);
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('c');
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			Channel* c = (Channel*)dest;
			ModResult res = ServerInstance->OnCheckExemption(user,c,"blockcolor");

			if (res == MOD_RES_ALLOW)
				return MOD_RES_PASSTHRU;

			if (!c->GetExtBanStatus(user, 'c').check(!c->IsModeSet('c')))
			{
				for (std::string::iterator i = text.begin(); i != text.end(); i++)
				{
					switch (*i)
					{
						case 2:
						case 3:
						case 15:
						case 21:
						case 22:
						case 31:
							user->WriteNumeric(404, "%s %s :Can't send colors to channel (+c set)",user->nick.c_str(), c->name.c_str());
							return MOD_RES_DENY;
						break;
					}
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual ~ModuleBlockColor()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides channel mode +c to block color",VF_VENDOR);
	}
};

MODULE_INIT(ModuleBlockColor)
