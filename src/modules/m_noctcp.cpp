/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004, 2006 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Provides support for unreal-style channel mode +C */

class NoCTCP : public ModeHandler
{
 public:
	NoCTCP(InspIRCd* Instance) : ModeHandler(Instance, 'C', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		if (adding)
		{
			if (!channel->IsModeSet('C'))
			{
				channel->SetMode('C',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('C'))
			{
				channel->SetMode('C',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleNoCTCP : public Module
{

	NoCTCP* nc;

 public:

	ModuleNoCTCP(InspIRCd* Me)
		: Module(Me)
	{

		nc = new NoCTCP(ServerInstance);
		if (!ServerInstance->Modes->AddMode(nc))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ~ModuleNoCTCP()
	{
		ServerInstance->Modes->DelMode(nc);
		delete nc;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreNotice(user,dest,target_type,text,status,exempt_list);
	}

	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			Channel* c = (Channel*)dest;

			if (CHANOPS_EXEMPT(ServerInstance, 'C') && c->GetStatus(user) == STATUS_OP)
			{
				return 0;
			}

			if (c->IsModeSet('C') || c->GetExtBanStatus(user, 'C') < 0)
			{
				if ((text.length()) && (text[0] == '\1'))
				{
					if (strncmp(text.c_str(),"\1ACTION ",8))
					{
						user->WriteNumeric(ERR_NOCTCPALLOWED, "%s %s :Can't send CTCP to channel (+C set)",user->nick.c_str(), c->name.c_str());
						return 1;
					}
				}
			}
		}
		return 0;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('C');
	}
};

MODULE_INIT(ModuleNoCTCP)
