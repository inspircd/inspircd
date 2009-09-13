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

class DelayJoinMode : public ModeHandler
{
 private:
	CUList empty;
 public:
	DelayJoinMode(InspIRCd* Instance, Module* Parent) : ModeHandler(Instance, Parent, 'D', 0, 0, false, MODETYPE_CHANNEL, false, 0, '@') {};

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
};

class ModuleDelayJoin : public Module
{
	DelayJoinMode djm;
 public:
	LocalIntExt unjoined;
	ModuleDelayJoin(InspIRCd* Me) : Module(Me), djm(Me, this), unjoined("delayjoin", this)
	{
		if (!ServerInstance->Modes->AddMode(&djm))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserJoin, I_OnUserPart, I_OnUserKick, I_OnUserQuit, I_OnNamesListItem, I_OnText, I_OnHostCycle };
		ServerInstance->Modules->Attach(eventlist, this, 7);
	}
	~ModuleDelayJoin();
	Version GetVersion();
	void OnNamesListItem(User* issuer, User* user, Channel* channel, std::string &prefixes, std::string &nick);
	void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent, bool created);
	void CleanUser(User* user);
	ModResult OnHostCycle(User* user);
	void OnUserPart(User* user, Channel* channel, std::string &partmessage, bool &silent);
	void OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent);
	void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message);
	void OnText(User* user, void* dest, int target_type, const std::string &text, char status, CUList &exempt_list);
	void WriteCommonFrom(User *user, Channel* channel, const char* text, ...) CUSTOM_PRINTF(4, 5);
};

/* $ModDesc: Allows for delay-join channels (+D) where users dont appear to join until they speak */

ModeAction DelayJoinMode::OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
{
	/* no change */
	if (channel->IsModeSet('D') == adding)
		return MODEACTION_DENY;

	if (!adding)
	{
		/*
		 * Make all users visible, as +D is being removed. If we don't do this,
		 * they remain permanently invisible on this channel!
		 */
		const UserMembList* names = channel->GetUsers();
		for (UserMembCIter n = names->begin(); n != names->end(); ++n)
			creator->OnText(n->first, channel, TYPE_CHANNEL, "", 0, empty);
	}
	channel->SetMode('D', adding);
	return MODEACTION_ALLOW;
}

ModuleDelayJoin::~ModuleDelayJoin()
{
	ServerInstance->Modes->DelMode(&djm);
}

Version ModuleDelayJoin::GetVersion()
{
	return Version("$Id$", VF_COMMON | VF_VENDOR);
}

void ModuleDelayJoin::OnNamesListItem(User* issuer, User* user, Channel* channel, std::string &prefixes, std::string &nick)
{
	if (!channel->IsModeSet('D'))
		return;

	if (nick.empty())
		return;

	/* don't prevent the user from seeing themself */
	if (issuer == user)
		return;

	Membership* memb = channel->GetUser(user);
	/* If the user is hidden by delayed join, hide them from the NAMES list */
	if (memb && unjoined.get(memb))
		nick.clear();
}

void ModuleDelayJoin::OnUserJoin(User* user, Channel* channel, bool sync, bool &silent, bool created)
{
	if (channel->IsModeSet('D'))
	{
		silent = true;
		/* Because we silenced the event, make sure it reaches the user whos joining (but only them of course) */
		user->WriteFrom(user, "JOIN %s", channel->name.c_str());

		Membership* memb = channel->GetUser(user);

		unjoined.set(memb, 1);
	}
}

void ModuleDelayJoin::OnUserPart(User* user, Channel* channel, std::string &partmessage, bool &silent)
{
	Membership* memb = channel->GetUser(user);
	if (unjoined.set(memb, 0))
	{
		silent = true;
		/* Because we silenced the event, make sure it reaches the user whos leaving (but only them of course) */
		user->WriteFrom(user, "PART %s%s%s", channel->name.c_str(), partmessage.empty() ? "" : " :", partmessage.empty() ? "" : partmessage.c_str());
	}
}

void ModuleDelayJoin::OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent)
{
	Membership* memb = chan->GetUser(user);
	if (unjoined.set(memb, 0))
	{
		silent = true;
		user->WriteFrom(source, "KICK %s %s %s", chan->name.c_str(), user->nick.c_str(), reason.c_str());
	}
}

ModResult ModuleDelayJoin::OnHostCycle(User* user)
{
	// TODO
	return MOD_RES_DENY;
}

void ModuleDelayJoin::OnUserQuit(User* user, const std::string &reason, const std::string &oper_message)
{
	Command* parthandler = ServerInstance->Parser->GetHandler("PART");
	if (!parthandler)
		return;
	for (UCListIter f = user->chans.begin(); f != user->chans.end(); f++)
	{
		Channel* chan = *f;
		Membership* memb = chan->GetUser(user);
		if (memb && unjoined.get(memb))
		{
			std::vector<std::string> parameters;
			parameters.push_back(chan->name);
			/* Send a fake PART from the channel, which will be silent */
			parthandler->Handle(parameters, user);
		}
	}
}

void ModuleDelayJoin::OnText(User* user, void* dest, int target_type, const std::string &text, char status, CUList &exempt_list)
{
	/* Server origin */
	if (!user)
		return;

	if (target_type != TYPE_CHANNEL)
		return;

	Channel* channel = (Channel*) dest;

	Membership* memb = channel->GetUser(user);
	if (!memb || !unjoined.set(memb, 0))
		return;

	/* Display the join to everyone else (the user who joined got it earlier) */
	channel->WriteAllExceptSender(user, false, 0, "JOIN %s", channel->name.c_str());

	std::string n = this->ServerInstance->Modes->ModeString(user, channel);
	if (n.length() > 0)
		this->WriteCommonFrom(user, channel, "MODE %s +%s", channel->name.c_str(), n.c_str());
}

MODULE_INIT(ModuleDelayJoin)

