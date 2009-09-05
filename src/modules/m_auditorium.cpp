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

/* $ModDesc: Allows for auditorium channels (+u) where nobody can see others joining and parting or the nick list */

class AuditoriumMode : public ModeHandler
{
 public:
	AuditoriumMode(InspIRCd* Instance, Module* Creator) : ModeHandler(Instance, Creator, 'u', 0, 0, false, MODETYPE_CHANNEL, false, 0, '@') { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (channel->IsModeSet('u') != adding)
		{
			channel->SetMode('u', adding);
			return MODEACTION_ALLOW;
		}
		else
		{
			return MODEACTION_DENY;
		}
	}
};

class ModuleAuditorium : public Module
{
 private:
	AuditoriumMode aum;
	bool ShowOps;
	bool OperOverride;
 public:
	ModuleAuditorium(InspIRCd* Me)
		: Module(Me), aum(Me, this)
	{
		if (!ServerInstance->Modes->AddMode(&aum))
			throw ModuleException("Could not add new modes!");

		OnRehash(NULL);

		Implementation eventlist[] = { I_OnUserJoin, I_OnUserPart, I_OnUserKick, I_OnUserQuit, I_OnNamesListItem, I_OnRehash, I_OnHostCycle };
		Me->Modules->Attach(eventlist, this, 7);

	}

	virtual ~ModuleAuditorium()
	{
		ServerInstance->Modes->DelMode(&aum);
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader conf(ServerInstance);
		ShowOps = conf.ReadFlag("auditorium", "showops", 0);
		OperOverride = conf.ReadFlag("auditorium", "operoverride", 0);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

	virtual void OnNamesListItem(User* issuer, User* user, Channel* channel, std::string &prefixes, std::string &nick)
	{
		if (!channel->IsModeSet('u'))
			return;

		/* Some module hid this from being displayed, dont bother */
		if (nick.empty())
			return;

		/* If user is oper and operoverride is on, don't touch the list */
		if (OperOverride && issuer->HasPrivPermission("channels/auspex"))
			return;

		if (ShowOps && (issuer != user) && (channel->GetStatus(user) < STATUS_OP))
		{
			/* Showops is set, hide all non-ops from the user, except themselves */
			nick.clear();
			return;
		}

		if (!ShowOps && (issuer != user))
		{
			/* ShowOps is not set, hide everyone except the user whos requesting NAMES */
			nick.clear();
			return;
		}
	}

	void WriteOverride(User* source, Channel* channel, const std::string &text)
	{
		if (!OperOverride)
			return;

		CUList *ulist = channel->GetUsers();
		for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			if (i->first->HasPrivPermission("channels/auspex") && source != i->first)
				if (!ShowOps || (ShowOps && channel->GetStatus(i->first) < STATUS_OP))
					i->first->WriteFrom(source, "%s",text.c_str());
		}
	}

	virtual void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent, bool created)
	{
		if (channel->IsModeSet('u'))
		{
			silent = true;
			/* Because we silenced the event, make sure it reaches the user whos joining (but only them of course) */
			user->WriteFrom(user, "JOIN %s", channel->name.c_str());
			if (ShowOps)
				channel->WriteAllExceptSender(user, false, channel->GetStatus(user) >= STATUS_OP ? 0 : '@', "JOIN %s", channel->name.c_str());
			WriteOverride(user, channel, "JOIN "+channel->name);
		}
	}

	void OnUserPart(User* user, Channel* channel, std::string &partmessage, bool &silent)
	{
		if (channel->IsModeSet('u'))
		{
			silent = true;
			/* Because we silenced the event, make sure it reaches the user whos leaving (but only them of course) */
			user->WriteFrom(user, "PART %s%s%s", channel->name.c_str(),
					partmessage.empty() ? "" : " :",
					partmessage.empty() ? "" : partmessage.c_str());
			if (ShowOps)
			{
				channel->WriteAllExceptSender(user, false, channel->GetStatus(user) >= STATUS_OP ? 0 : '@', "PART %s%s%s", channel->name.c_str(), partmessage.empty() ? "" : " :",
						partmessage.empty() ? "" : partmessage.c_str());
			}
			WriteOverride(user, channel, "PART " + channel->name + (partmessage.empty() ? "" : (" :" + partmessage)));
		}
	}

	void OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent)
	{
		if (chan->IsModeSet('u'))
		{
			silent = true;
			/* Send silenced event only to the user being kicked and the user doing the kick */
			source->WriteFrom(source, "KICK %s %s :%s", chan->name.c_str(), user->nick.c_str(), reason.c_str());
			if (ShowOps)
				chan->WriteAllExceptSender(source, false, chan->GetStatus(user) >= STATUS_OP ? 0 : '@', "KICK %s %s :%s", chan->name.c_str(), user->nick.c_str(), reason.c_str());
			if ((!ShowOps) || (chan->GetStatus(user) < STATUS_OP)) /* make sure the target gets the event */
				user->WriteFrom(source, "KICK %s %s :%s", chan->name.c_str(), user->nick.c_str(), reason.c_str());
			WriteOverride(source, chan, "KICK " + chan->name + " " + user->nick + " :" + reason);
		}
	}

	ModResult OnHostCycle(User* user)
	{
		for (UCListIter f = user->chans.begin(); f != user->chans.end(); f++)
			if (f->first->IsModeSet('u'))
				return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}

	void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message)
	{
		Command* parthandler = ServerInstance->Parser->GetHandler("PART");
		std::vector<std::string> to_leave;
		if (parthandler)
		{
			for (UCListIter f = user->chans.begin(); f != user->chans.end(); f++)
			{
				if (f->first->IsModeSet('u'))
					to_leave.push_back(f->first->name);
			}
			/* We cant do this neatly in one loop, as we are modifying the map we are iterating */
			for (std::vector<std::string>::iterator n = to_leave.begin(); n != to_leave.end(); n++)
			{
				std::vector<std::string> parameters;
				parameters.push_back(*n);
				/* This triggers our OnUserPart, above, making the PART silent */
				parthandler->Handle(parameters, user);
			}
		}
	}
};

MODULE_INIT(ModuleAuditorium)
