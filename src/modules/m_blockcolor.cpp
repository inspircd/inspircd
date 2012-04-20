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

/* $ModDesc: Provides support for unreal-style channel mode +c */

/** Handles the +c channel mode
 */
class BlockColor : public SimpleChannelModeHandler
{
 public:
	BlockColor(InspIRCd* Instance) : SimpleChannelModeHandler(Instance, 'c') { }
};

class ModuleBlockColor : public Module
{
	bool AllowChanOps;
	BlockColor *bc;
 public:

	ModuleBlockColor(InspIRCd* Me) : Module(Me)
	{
		bc = new BlockColor(ServerInstance);
		if (!ServerInstance->Modes->AddMode(bc))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('c');
	}

	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			Channel* c = (Channel*)dest;

			if (CHANOPS_EXEMPT(ServerInstance, 'c') && c->GetStatus(user) == STATUS_OP)
			{
				return 0;
			}

			if(c->IsModeSet('c') || c->GetExtBanStatus(user, 'c') < 0)
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
							return 1;
						break;
					}
				}
			}
		}
		return 0;
	}

	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual ~ModuleBlockColor()
	{
		ServerInstance->Modes->DelMode(bc);
		delete bc;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_COMMON|VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleBlockColor)
