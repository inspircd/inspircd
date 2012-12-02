/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

class ModeChannelHalfOp : public ModeHandler
{
 public:
	ModeChannelHalfOp(Module* parent);
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	unsigned int GetPrefixRank();
	void RemoveMode(Channel* channel, irc::modestacker* stack = NULL);
	void RemoveMode(User* user, irc::modestacker* stack = NULL);

	ModResult AccessCheck(User* src, Channel*, std::string& value, bool adding)
	{
		if (!adding && src->nick == value)
			return MOD_RES_ALLOW;
		return MOD_RES_PASSTHRU;
	}
};

ModeChannelHalfOp::ModeChannelHalfOp(Module* parent) : ModeHandler(parent, "halfop", 'h', PARAM_ALWAYS, MODETYPE_CHANNEL)
{
	list = true;
	prefix = '%';
	levelrequired = OP_VALUE;
	m_paramtype = TR_NICK;
}

unsigned int ModeChannelHalfOp::GetPrefixRank()
{
	return HALFOP_VALUE;
}

void ModeChannelHalfOp::RemoveMode(Channel* channel, irc::modestacker* stack)
{
	const UserMembList* clist = channel->GetUsers();

	for (UserMembCIter i = clist->begin(); i != clist->end(); i++)
	{
		if (stack)
		{
			stack->Push(this->GetModeChar(), i->first->nick);
		}
		else
		{
			std::vector<std::string> parameters;
			parameters.push_back(channel->name);
			parameters.push_back("-h");
			parameters.push_back(i->first->nick);
			ServerInstance->SendMode(parameters, ServerInstance->FakeClient);
		}
	}

}

void ModeChannelHalfOp::RemoveMode(User*, irc::modestacker* stack)
{
}

ModeAction ModeChannelHalfOp::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
{
	return MODEACTION_ALLOW;
}

class ModuleHalfop : public Module
{
	ModeChannelHalfOp mh;
 public:
	ModuleHalfop() : mh(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(mh);
	}

	Version GetVersion()
	{
		return Version("Channel half-operator mode provider", VF_VENDOR);
	}
};

MODULE_INIT(ModuleHalfop)
