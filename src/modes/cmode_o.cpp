/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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
#include "configreader.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modules.h"
#include "modes/cmode_o.h"

ModeChannelOp::ModeChannelOp() : ModeHandler(NULL, "op", 'o', PARAM_ALWAYS, MODETYPE_CHANNEL)
{
	list = true;
	prefix = '@';
	levelrequired = OP_VALUE;
	m_paramtype = TR_NICK;
}

unsigned int ModeChannelOp::GetPrefixRank()
{
	return OP_VALUE;
}

void ModeChannelOp::RemoveMode(Channel* channel, irc::modestacker* stack)
{
	const UserMembList* clist = channel->GetUsers();

	for (UserMembCIter i = clist->begin(); i != clist->end(); i++)
	{
		if (stack)
			stack->Push(this->GetModeChar(), i->first->nick);
		else
		{
			std::vector<std::string> parameters;
			parameters.push_back(channel->name);
			parameters.push_back("-o");
			parameters.push_back(i->first->nick);
			ServerInstance->SendMode(parameters, ServerInstance->FakeClient);
		}
	}
}

void ModeChannelOp::RemoveMode(User*, irc::modestacker* stack)
{
}

ModeAction ModeChannelOp::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
{
	return MODEACTION_ALLOW;
}
