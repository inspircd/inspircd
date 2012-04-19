
/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
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
#include "modes/cmode_v.h"

ModeChannelVoice::ModeChannelVoice() : ModeHandler(NULL, "voice", 'v', PARAM_ALWAYS, MODETYPE_CHANNEL)
{
	list = true;
	prefix = '+';
	levelrequired = HALFOP_VALUE;
	m_paramtype = TR_NICK;
}

unsigned int ModeChannelVoice::GetPrefixRank()
{
	return VOICE_VALUE;
}

void ModeChannelVoice::RemoveMode(Channel* channel, irc::modestacker* stack)
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
			parameters.push_back("-v");
			parameters.push_back(i->first->nick);
			ServerInstance->SendMode(parameters, ServerInstance->FakeClient);
		}
	}
}

void ModeChannelVoice::RemoveMode(User*, irc::modestacker* stack)
{
}

ModeAction ModeChannelVoice::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
{
	return MODEACTION_ALLOW;
}
