/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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


/* $ModDesc: Channel half-operator mode provider */

#include "inspircd.h"

class ModeChannelHalfOp : public PrefixModeHandler
{
 public:
	ModeChannelHalfOp(Module* parent);
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	unsigned int GetPrefixRank();

	void AccessCheck(ModePermissionData& perm)
	{
		if (!perm.mc.adding && perm.source == perm.user)
			perm.result = MOD_RES_ALLOW;
	}
};

ModeChannelHalfOp::ModeChannelHalfOp(Module* parent) : PrefixModeHandler(parent, "halfop", 'h')
{
	prefix = '%';
	levelrequired = OP_VALUE;
	fixed_letter = false;
}

unsigned int ModeChannelHalfOp::GetPrefixRank()
{
	return HALFOP_VALUE;
}

ModeAction ModeChannelHalfOp::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
{
	return MODEACTION_ALLOW;
}

class ModuleHalfop : public Module
{
	ModeChannelHalfOp mh;
 public:
	ModuleHalfop() : mh(this) {}

	void init()
	{
		ServerInstance->Logs->Log("m_halfop", DEFAULT, "m_halfop is deprecated as of 2.1 in favour of m_customprefix, please consider switching");
		ServerInstance->Modules->AddService(mh);
	}

	Version GetVersion()
	{
		return Version("Channel half-operator mode provider", VF_VENDOR);
	}
};

MODULE_INIT(ModuleHalfop)
