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

class DelayMsgMode : public ParamMode<DelayMsgMode, LocalIntExt>
{
 public:
	LocalIntExt jointime;
	DelayMsgMode(Module* Parent)
		: ParamMode<DelayMsgMode, LocalIntExt>(Parent, "delaymsg", 'd')
		, jointime("delaymsg", ExtensionItem::EXT_MEMBERSHIP, Parent)
	{
		ranktoset = ranktounset = OP_VALUE;
	}

	bool ResolveModeConflict(std::string& their_param, const std::string& our_param, Channel*) CXX11_OVERRIDE
	{
		return (atoi(their_param.c_str()) < atoi(our_param.c_str()));
	}

	ModeAction OnSet(User* source, Channel* chan, std::string& parameter) CXX11_OVERRIDE;
	void OnUnset(User* source, Channel* chan);

	void SerializeParam(Channel* chan, intptr_t n, std::string& out)
	{
		out += ConvToStr(n);
	}
};

class ModuleDelayMsg : public Module
{
	DelayMsgMode djm;
	bool allownotice;
 public:
	ModuleDelayMsg() : djm(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE;
	void OnUserJoin(Membership* memb, bool sync, bool created, CUList&) CXX11_OVERRIDE;
	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE;
	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE;
};

ModeAction DelayMsgMode::OnSet(User* source, Channel* chan, std::string& parameter)
{
	// Setting a new limit, sanity check
	intptr_t limit = ConvToNum<intptr_t>(parameter);
	if (limit <= 0)
		limit = 1;

	ext.set(chan, limit);
	return MODEACTION_ALLOW;
}

void DelayMsgMode::OnUnset(User* source, Channel* chan)
{
	/*
	 * Clean up metadata
	 */
	const Channel::MemberMap& users = chan->GetUsers();
	for (Channel::MemberMap::const_iterator n = users.begin(); n != users.end(); ++n)
		jointime.set(n->second, 0);
}

Version ModuleDelayMsg::GetVersion()
{
	return Version("Provides channelmode +d <int>, to deny messages to a channel until <int> seconds.", VF_VENDOR);
}

void ModuleDelayMsg::OnUserJoin(Membership* memb, bool sync, bool created, CUList&)
{
	if ((IS_LOCAL(memb->user)) && (memb->chan->IsModeSet(djm)))
	{
		djm.jointime.set(memb, ServerInstance->Time());
	}
}

ModResult ModuleDelayMsg::OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details)
{
	if (!IS_LOCAL(user))
		return MOD_RES_PASSTHRU;

	if ((target.type != MessageTarget::TYPE_CHANNEL) || ((!allownotice) && (details.type == MSG_NOTICE)))
		return MOD_RES_PASSTHRU;

	Channel* channel = target.Get<Channel>();
	Membership* memb = channel->GetUser(user);

	if (!memb)
		return MOD_RES_PASSTHRU;

	time_t ts = djm.jointime.get(memb);

	if (ts == 0)
		return MOD_RES_PASSTHRU;

	int len = djm.ext.get(channel);

	if ((ts + len) > ServerInstance->Time())
	{
		if (channel->GetPrefixValue(user) < VOICE_VALUE)
		{
			user->WriteNumeric(ERR_CANNOTSENDTOCHAN, channel->name, InspIRCd::Format("You must wait %d seconds after joining to send to channel (+d)", len));
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

void ModuleDelayMsg::ReadConfig(ConfigStatus& status)
{
	ConfigTag* tag = ServerInstance->Config->ConfValue("delaymsg");
	allownotice = tag->getBool("allownotice", true);
}

MODULE_INIT(ModuleDelayMsg)
