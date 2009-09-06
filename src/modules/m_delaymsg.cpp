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
	DelayMsgMode(InspIRCd* Instance, Module* Parent) : ModeHandler(Instance, Parent, 'd', 1, 0, false, MODETYPE_CHANNEL, false, 0, '@') {};

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
	ModuleDelayMsg(InspIRCd* Me) : Module(Me), djm(Me, this)
	{
		if (!ServerInstance->Modes->AddMode(&djm))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserJoin, I_OnUserPart, I_OnUserKick, I_OnCleanup, I_OnUserPreMessage};
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}
	virtual ~ModuleDelayMsg();
	virtual Version GetVersion();
	void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent, bool created);
	void OnUserPart(User* user, Channel* channel, std::string &partmessage, bool &silent);
	void OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent);
	void OnCleanup(int target_type, void* item);
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
		CUList* names = channel->GetUsers();
		for (CUListIter n = names->begin(); n != names->end(); ++n)
			n->first->Shrink("delaymsg_" + channel->name);
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
	return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
}

void ModuleDelayMsg::OnUserJoin(User* user, Channel* channel, bool sync, bool &silent, bool created)
{
	if (channel->IsModeSet('d'))
		user->Extend("delaymsg_"+channel->name, reinterpret_cast<char*>(ServerInstance->Time()));
}

void ModuleDelayMsg::OnUserPart(User* user, Channel* channel, std::string &partmessage, bool &silent)
{
	user->Shrink("delaymsg_"+channel->name);
}

void ModuleDelayMsg::OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent)
{
	user->Shrink("delaymsg_"+chan->name);
}

void ModuleDelayMsg::OnCleanup(int target_type, void* item)
{
	if (target_type == TYPE_USER)
	{
		User* user = (User*)item;
		for (UCListIter f = user->chans.begin(); f != user->chans.end(); f++)
		{
			Channel* chan = f->first;
			user->Shrink("delaymsg_"+chan->name);
		}
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

	void* jointime_as_ptr;

	if (!user->GetExt("delaymsg_"+channel->name, jointime_as_ptr))
		return MOD_RES_PASSTHRU;

	time_t jointime = reinterpret_cast<time_t>(jointime_as_ptr);

	std::string len = channel->GetModeParameter('d');

	if (jointime + atoi(len.c_str()) > ServerInstance->Time())
	{
		if (channel->GetStatus(user) < STATUS_VOICE)
		{
			user->WriteNumeric(404, "%s %s :You must wait %s seconds after joining to send to channel (+d)",
				user->nick.c_str(), channel->name.c_str(), len.c_str());
			return MOD_RES_DENY;
		}
	}
	else
	{
		/* Timer has expired, we can stop checking now */
		user->Shrink("delaymsg_"+channel->name);
	}
	return MOD_RES_PASSTHRU;
}

MODULE_INIT(ModuleDelayMsg)

