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


#include "inspircd.h"

/* $ModDesc: Provides channelmode +d <int>, to deny messages to a channel until <int> seconds. */

class DelayMsgMode : public ModeHandler
{
 public:
	LocalIntExt jointime;
	DelayMsgMode(Module* Parent) : ModeHandler(Parent, "delaymsg", 'd', PARAM_SETONLY, MODETYPE_CHANNEL)
		, jointime("delaymsg", Parent)
	{
		levelrequired = OP_VALUE;
	}

	bool ResolveModeConflict(std::string &their_param, const std::string &our_param, Channel*)
	{
		return (atoi(their_param.c_str()) < atoi(our_param.c_str()));
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
};

class ModuleDelayMsg : public Module
{
	DelayMsgMode djm;
 public:
	ModuleDelayMsg() : djm(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->Modules->AddService(djm);
		ServerInstance->Modules->AddService(djm.jointime);
		Implementation eventlist[] = { I_OnUserJoin, I_OnUserPreMessage};
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}
	Version GetVersion() CXX11_OVERRIDE;
	void OnUserJoin(Membership* memb, bool sync, bool created, CUList&) CXX11_OVERRIDE;
	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list) CXX11_OVERRIDE;
};

ModeAction DelayMsgMode::OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
{
	if (adding)
	{
		if ((channel->IsModeSet('d')) && (channel->GetModeParameter('d') == parameter))
			return MODEACTION_DENY;

		/* Setting a new limit, sanity check */
		long limit = atoi(parameter.c_str());

		/* Wrap low values at 32768 */
		if (limit < 0)
			limit = 0x7FFF;

		parameter = ConvToStr(limit);
	}
	else
	{
		if (!channel->IsModeSet('d'))
			return MODEACTION_DENY;

		/*
		 * Clean up metadata
		 */
		const UserMembList* names = channel->GetUsers();
		for (UserMembCIter n = names->begin(); n != names->end(); ++n)
			jointime.set(n->second, 0);
	}
	channel->SetModeParam('d', adding ? parameter : "");
	return MODEACTION_ALLOW;
}

Version ModuleDelayMsg::GetVersion()
{
	return Version("Provides channelmode +d <int>, to deny messages to a channel until <int> seconds.", VF_VENDOR);
}

void ModuleDelayMsg::OnUserJoin(Membership* memb, bool sync, bool created, CUList&)
{
	if ((IS_LOCAL(memb->user)) && (memb->chan->IsModeSet('d')))
	{
		djm.jointime.set(memb, ServerInstance->Time());
	}
}

ModResult ModuleDelayMsg::OnUserPreMessage(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list)
{
	/* Server origin */
	if ((!user) || (!IS_LOCAL(user)))
		return MOD_RES_PASSTHRU;

	if (target_type != TYPE_CHANNEL)
		return MOD_RES_PASSTHRU;

	Channel* channel = (Channel*) dest;
	Membership* memb = channel->GetUser(user);

	if (!memb)
		return MOD_RES_PASSTHRU;

	time_t ts = djm.jointime.get(memb);

	if (ts == 0)
		return MOD_RES_PASSTHRU;

	std::string len = channel->GetModeParameter('d');

	if (ts + atoi(len.c_str()) > ServerInstance->Time())
	{
		if (channel->GetPrefixValue(user) < VOICE_VALUE)
		{
			user->WriteNumeric(404, "%s %s :You must wait %s seconds after joining to send to channel (+d)",
				user->nick.c_str(), channel->name.c_str(), len.c_str());
			return MOD_RES_DENY;
		}
	}
	else
	{
		/* Timer has expired, we can stop checking now */
		djm.jointime.set(memb, 0);
	}
	return MOD_RES_PASSTHRU;
}

MODULE_INIT(ModuleDelayMsg)
