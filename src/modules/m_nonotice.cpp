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

/* $ModDesc: Provides support for unreal-style channel mode +T */

class NoNotice : public SimpleChannelModeHandler
{
 public:
	NoNotice(InspIRCd* Instance) : SimpleChannelModeHandler(Instance, 'T') { }
};

class ModuleNoNotice : public Module
{

	NoNotice* nt;
 public:

	ModuleNoNotice(InspIRCd* Me)
		: Module(Me)
	{

		nt = new NoNotice(ServerInstance);
		if (!ServerInstance->Modes->AddMode(nt))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('T');
	}

	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			Channel* c = (Channel*)dest;
			if (c->IsModeSet('T') || c->GetExtBanStatus(user, 'T') < 0)
			{
				if (ServerInstance->ULine(user->server))
				{
					// ulines are exempt.
					return 0;
				}
				else if (CHANOPS_EXEMPT(ServerInstance, 'T') && c->GetStatus(user) == STATUS_OP)
				{
					// channel ops are exempt if set in conf.
					return 0;
				}
				else
				{
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, "%s %s :Can't send NOTICE to channel (+T set)",user->nick.c_str(), c->name.c_str());
					return 1;
				}
			}
		}
		return 0;
	}

	virtual ~ModuleNoNotice()
	{
		ServerInstance->Modes->DelMode(nt);
		delete nt;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleNoNotice)
