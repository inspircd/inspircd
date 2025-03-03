/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 iwalkalone <iwalkalone69@gmail.com>
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2017-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
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


#include "inspircd.h"
#include "modules/ctctags.h"
#include "modules/exemption.h"
#include "timeutils.h"
#include "numerichelper.h"

class DelayMsgMode final
	: public ParamMode<DelayMsgMode, IntExtItem>
{
public:
	IntExtItem jointime;
	DelayMsgMode(Module* Parent)
		: ParamMode<DelayMsgMode, IntExtItem>(Parent, "delaymsg", 'd')
		, jointime(Parent, "delaymsg", ExtensionType::MEMBERSHIP)
	{
		ranktoset = ranktounset = OP_VALUE;
		syntax = "<seconds>";
	}

	bool ResolveModeConflict(const std::string& their_param, const std::string& our_param, Channel*) override
	{
		return ConvToNum<intptr_t>(their_param) < ConvToNum<intptr_t>(our_param);
	}

	bool OnSet(User* source, Channel* chan, std::string& parameter) override;
	void OnUnset(User* source, Channel* chan) override;

	void SerializeParam(Channel* chan, intptr_t n, std::string& out)
	{
		out += ConvToStr(n);
	}
};

class ModuleDelayMsg final
	: public Module
	, public CTCTags::EventListener
{
private:
	DelayMsgMode djm;
	CheckExemption::EventProvider exemptionprov;
	ModResult HandleMessage(User* user, const MessageTarget& target);

public:
	ModuleDelayMsg()
		: Module(VF_VENDOR, "Adds channel mode d (delaymsg) which prevents newly joined users from speaking until the specified number of seconds have passed.")
		, CTCTags::EventListener(this)
		, djm(this)
		, exemptionprov(this)
	{
	}

	void OnUserJoin(Membership* memb, bool sync, bool created, CUList&) override;
	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override;
	ModResult OnUserPreTagMessage(User* user, MessageTarget& target, CTCTags::TagMessageDetails& details) override;
};

bool DelayMsgMode::OnSet(User* source, Channel* chan, std::string& parameter)
{
	// Setting a new limit, sanity check
	intptr_t limit = ConvToNum<intptr_t>(parameter);
	if (limit <= 0)
		limit = 1;

	ext.Set(chan, limit);
	return true;
}

void DelayMsgMode::OnUnset(User* source, Channel* chan)
{
	for (const auto& [_, memb] : chan->GetUsers())
		jointime.Unset(memb);
}

void ModuleDelayMsg::OnUserJoin(Membership* memb, bool sync, bool created, CUList&)
{
	if ((IS_LOCAL(memb->user)) && (memb->chan->IsModeSet(djm)))
	{
		djm.jointime.Set(memb, ServerInstance->Time());
	}
}

ModResult ModuleDelayMsg::OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details)
{
	return HandleMessage(user, target);
}

ModResult ModuleDelayMsg::OnUserPreTagMessage(User* user, MessageTarget& target, CTCTags::TagMessageDetails& details)
{
	return HandleMessage(user, target);
}

ModResult ModuleDelayMsg::HandleMessage(User* user, const MessageTarget& target)
{
	if (!IS_LOCAL(user) || target.type != MessageTarget::TYPE_CHANNEL)
		return MOD_RES_PASSTHRU;

	auto* channel = target.Get<Channel>();
	Membership* memb = channel->GetUser(user);

	if (!memb)
		return MOD_RES_PASSTHRU;

	time_t ts = djm.jointime.Get(memb);

	if (ts == 0)
		return MOD_RES_PASSTHRU;

	intptr_t len = djm.ext.Get(channel);

	if ((ts + len) > ServerInstance->Time())
	{
		ModResult res = exemptionprov.Check(user, channel, "delaymsg");
		if (res == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		if (user->HasPrivPermission("channels/ignore-delaymsg"))
			return MOD_RES_PASSTHRU;

		const std::string message = FMT::format("You cannot send messages to this channel until you have been a member for {}.",
			Duration::ToHuman(len));
		user->WriteNumeric(Numerics::CannotSendTo(channel, message));
		return MOD_RES_DENY;
	}
	else
	{
		/* Timer has expired, we can stop checking now */
		djm.jointime.Unset(memb);
	}
	return MOD_RES_PASSTHRU;
}

MODULE_INIT(ModuleDelayMsg)
