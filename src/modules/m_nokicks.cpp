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

/* $ModDesc: Provides support for unreal-style channel mode +Q */

class NoKicks : public SimpleChannelModeHandler
{
 public:
	NoKicks(InspIRCd* Instance) : SimpleChannelModeHandler(Instance, 'Q') { }
};

class ModuleNoKicks : public Module
{
	NoKicks* nk;

 public:
	ModuleNoKicks(InspIRCd* Me)
		: Module(Me)
	{
		nk = new NoKicks(ServerInstance);
		if (!ServerInstance->Modes->AddMode(nk))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnAccessCheck, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('Q');
	}

	virtual int OnAccessCheck(User* source,User* dest,Channel* channel,int access_type)
	{
		if (access_type == AC_KICK)
		{
			if (channel->IsModeSet('Q') || channel->GetExtBanStatus(source, 'Q') < 0)
			{
				if ((ServerInstance->ULine(source->nick.c_str())) || (ServerInstance->ULine(source->server)) || (!*source->server))
				{
					// ulines can still kick with +Q in place
					return ACR_ALLOW;
				}
				else
				{
					// nobody else can (not even opers with override, and founders)
					source->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :Can't kick user %s from channel (+Q set)",source->nick.c_str(), channel->name.c_str(), dest->nick.c_str());
					return ACR_DENY;
				}
			}
		}
		return ACR_DEFAULT;
	}

	virtual ~ModuleNoKicks()
	{
		ServerInstance->Modes->DelMode(nk);
		delete nk;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};


MODULE_INIT(ModuleNoKicks)
