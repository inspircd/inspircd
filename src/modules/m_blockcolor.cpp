/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2011 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
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

/* $ModDesc: Provides support for unreal-style channel mode +c */

/** Handles the +c channel mode
 */
class BlockColor : public SimpleChannelModeHandler
{
 public:
	BlockColor(Module* Creator) : SimpleChannelModeHandler(Creator, "blockcolor", 'c') { fixed_letter = false; }
};

class ModuleBlockColor : public Module
{
	bool AllowChanOps;
	BlockColor bc;
 public:

	ModuleBlockColor() : bc(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(bc);
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_OnUserPart, I_On005Numeric };
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

			if (ServerInstance->CheckExemption(user,c,"blockcolor") != MOD_RES_ALLOW && !c->GetExtBanStatus(user, 'c').check(!c->IsModeSet(&bc)))
				if (text.find_first_of("\x02\x03\x0f\x15\x16\x1f") != std::string::npos)
				{
					user->WriteNumeric(404, "%s %s :Can't send colors to channel (+c set)",user->nick.c_str(), c->name.c_str());
					return MOD_RES_DENY;
				}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual void OnUserPart(Membership* memb, std::string &partmessage, CUList&)
	{
		if (!IS_LOCAL(memb->user))
			return;

		if (ServerInstance->CheckExemption(memb->user,memb->chan,"blockcolor") == MOD_RES_ALLOW)
			return;

		if (!memb->chan->GetExtBanStatus(memb->user, 'c').check(!memb->chan->IsModeSet(&bc)))
			if (partmessage.find_first_of("\x02\x03\x0f\x15\x16\x1f") != std::string::npos)
				partmessage = "";
	}

	virtual ~ModuleBlockColor()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for unreal-style channel mode +c",VF_VENDOR);
	}
};

MODULE_INIT(ModuleBlockColor)
