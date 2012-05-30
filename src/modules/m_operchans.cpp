/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDesc: Provides support for oper-only chans via the +O channel mode and 'O' extban */

class OperChans : public SimpleChannelModeHandler
{
 public:
	/* This is an oper-only mode */
	OperChans(Module* Creator) : SimpleChannelModeHandler(Creator, "operonly", 'O') { fixed_letter = false; }
};

class ModuleOperChans : public Module
{
	OperChans oc;
 public:
	ModuleOperChans() : oc(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(oc);
		Implementation eventlist[] = { I_OnCheckBan, I_On005Numeric, I_OnCheckJoin };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnCheckJoin(ChannelPermissionData& join)
	{
		if (join.chan && join.result == MOD_RES_PASSTHRU && join.chan->IsModeSet(&oc) && !IS_OPER(join.user))
		{
			join.ErrorNumeric(ERR_CANTJOINOPERSONLY, "%s :Only IRC operators may join %s (+O is set)",
				join.chan->name.c_str(), join.chan->name.c_str());
			join.result = MOD_RES_DENY;
		}
	}

	ModResult OnCheckBan(User *user, Channel *c, const std::string& mask)
	{
		if (mask[0] == 'O' && mask[1] == ':')
		{
			if (IS_OPER(user) && InspIRCd::Match(user->oper->name, mask.substr(2)))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('O');
	}

	~ModuleOperChans()
	{
	}

	Version GetVersion()
	{
		return Version("Provides support for oper-only chans via the +O channel mode and 'O' extban", VF_VENDOR);
	}
};

MODULE_INIT(ModuleOperChans)
