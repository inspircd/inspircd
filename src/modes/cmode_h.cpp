/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "configreader.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modules.h"
#include "modes/cmode_h.h"

ModeChannelHalfOp::ModeChannelHalfOp() : ModeHandler(NULL, 'h', PARAM_ALWAYS, MODETYPE_CHANNEL)
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

ModePair ModeChannelHalfOp::ModeSet(User*, User*, Channel* channel, const std::string &parameter)
{
	User* x = ServerInstance->FindNick(parameter);
	if (x)
	{
		Membership* memb = channel->GetUser(x);
		if (memb && memb->hasMode('h'))
		{
			return std::make_pair(true, x->nick);
		}
		else
		{
			return std::make_pair(false, x->nick);
		}
	}
	return std::make_pair(false, parameter);
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
