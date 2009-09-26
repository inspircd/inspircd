/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <stdarg.h>

/* $ModDesc: Provides channelmode +d <int>, to deny messages to a channel until <int> seconds. */

class DelayMsgMode : public ModeHandler
{
 private:
	CUList empty;
 public:
	LocalIntExt jointime;
	DelayMsgMode(Module* Parent) : ModeHandler(Parent, 'd', PARAM_SETONLY, MODETYPE_CHANNEL)
		, jointime("delaymsg", Parent)
	{
		levelrequired = OP_VALUE;
	}

	ModePair ModeSet(User*, User*, Channel* channel, const std::string &parameter)
	{
		std::string climit = channel->GetModeParameter('d');
		if (!climit.empty())
		{
			return std::make_pair(true, climit);
		}
		else
		{
			return std::make_pair(false, parameter);
		}
	}

	bool CheckTimeStamp(std::string &their_param, const std::string &our_param, Channel*)
	{
		return (atoi(their_param.c_str()) < atoi(our_param.c_str()));
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
};

class ModuleDelayMsg : public Module
{
 private:
	DelayMsgMode djm;
 public:
	ModuleDelayMsg() : djm(this)
	{
		if (!ServerInstance->Modes->AddMode(&djm))
			throw ModuleException("Could not add new modes!");
		Extensible::Register(&djm.jointime);
		Implementation eventlist[] = { I_OnUserJoin, I_OnUserPreMessage};
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}
	~ModuleDelayMsg();
	Version GetVersion();
	void OnUserJoin(Membership* memb, bool sync, bool created, CUList&);
	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list);
};

ModeAction DelayMsgMode::OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
{
	if (adding)
	{
		/* Setting a new limit, sanity check */
		long limit = atoi(parameter.c_str());

		/* Wrap low values at 32768 */
		if (limit < 0)
			limit = 0x7FFF;

		parameter = ConvToStr(limit);
	}
	else
	{
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

ModuleDelayMsg::~ModuleDelayMsg()
{
	ServerInstance->Modes->DelMode(&djm);
}

Version ModuleDelayMsg::GetVersion()
{
	return Version("Provides channelmode +d <int>, to deny messages to a channel until <int> seconds.", VF_COMMON | VF_VENDOR, API_VERSION);
}

void ModuleDelayMsg::OnUserJoin(Membership* memb, bool sync, bool created, CUList&)
{
	if (memb->chan->IsModeSet('d'))
	{
		djm.jointime.set(memb, ServerInstance->Time());
	}
}

ModResult ModuleDelayMsg::OnUserPreMessage(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list)
{
	/* Server origin */
	if (!user)
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

