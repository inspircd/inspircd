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
	NoCTCP(Module* Creator) : ModeHandler(Creator, "noctcp", 'C', PARAM_NONE, MODETYPE_CHANNEL) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
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

	NoCTCP nc;

 public:

	ModuleNoCTCP()
		: nc(this)
	{
		if (!ServerInstance->Modes->AddMode(&nc))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ~ModuleNoCTCP()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for unreal-style channel mode +C", VF_VENDOR);
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreNotice(user,dest,target_type,text,status,exempt_list);
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			Channel* c = (Channel*)dest;
			ModResult res = ServerInstance->OnCheckExemption(user,c,"noctcp");

			if (res == MOD_RES_ALLOW)
				return MOD_RES_PASSTHRU;

			if (!c->GetExtBanStatus(user, 'C').check(!c->IsModeSet('C')))
			{
				if ((text.length()) && (text[0] == '\1'))
				{
					if (strncmp(text.c_str(),"\1ACTION ",8))
					{
						user->WriteNumeric(ERR_NOCTCPALLOWED, "%s %s :Can't send CTCP to channel (+C set)",user->nick.c_str(), c->name.c_str());
						return MOD_RES_DENY;
					}
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('C');
	}
};

MODULE_INIT(ModuleNoCTCP)
