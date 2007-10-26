/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Allows for delay-join channels (+D) where users dont appear to join until they speak */

class DelayJoinMode : public ModeHandler
{
 public:
	DelayJoinMode(InspIRCd* Instance) : ModeHandler(Instance, 'D', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (channel->IsModeSet('D') != adding)
		{
			if (IS_LOCAL(source) && (channel->GetStatus(source) < STATUS_OP))
			{
				source->WriteServ("482 %s %s :Only channel operators may %sset channel mode +D", source->nick, channel->name, adding ? "" : "un");
				return MODEACTION_DENY;
			}
			else
			{
				channel->SetMode('D', adding);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			return MODEACTION_DENY;
		}
	}
};

class ModuleDelayJoin : public Module
{
 private:
	DelayJoinMode* djm;
	CUList nl;
 public:
	ModuleDelayJoin(InspIRCd* Me)
		: Module(Me)
	{
		djm = new DelayJoinMode(ServerInstance);
		if (!ServerInstance->AddMode(djm))
			throw ModuleException("Could not add new modes!");
	}
	
	virtual ~ModuleDelayJoin()
	{
		ServerInstance->Modes->DelMode(djm);
		DELETE(djm);
	}

	Priority Prioritize()
	{
		/* To ensure that we get priority over namesx for names list generation on +u channels */
		return (Priority)ServerInstance->Modules->PriorityBefore("m_namesx.so");
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnUserJoin] = List[I_OnUserPart] = List[I_OnUserKick] = List[I_OnUserQuit] = List[I_OnUserList] = List[I_OnText] = 1;
	}

	virtual int OnUserList(User* user, Channel* Ptr, CUList* &nameslist)
	{
		CUList* newlist = nameslist ? nameslist : Ptr->GetUsers();

		nl.clear();

		/* For +D channels ... */
		if (Ptr->IsModeSet('D'))
		{
			std::string key("delayjoin_");
			key.append(Ptr->name);
			ServerInstance->Log(DEBUG,"Key: %s", key.c_str());
			/* Modify the names list, erasing users with the delay join metadata
			 * for this channel (havent spoken yet)
			 */
			for (CUListIter n = newlist->begin(); n != newlist->end(); ++n)
			{
				ServerInstance->Log(DEBUG,"Item: %s", n->first->nick);
				if (!n->first->GetExt(key))
				{
					nl.insert(*n);
					ServerInstance->Log(DEBUG,"Spoken: %s", n->first->nick);
				}
			}
			ServerInstance->Log(DEBUG,"Insert self");
			nl[user] = user->nick;
			nameslist = &nl;
		}
		return 0;
	}

	virtual void OnUserJoin(User* user, Channel* channel, bool &silent)
	{
		if (channel->IsModeSet('D'))
		{
			silent = true;
			/* Because we silenced the event, make sure it reaches the user whos joining (but only them of course) */
			user->WriteFrom(user, "JOIN %s", channel->name);

			/* This metadata tells the module the user is delayed join on this specific channel */
			user->Extend(std::string("delayjoin_")+channel->name);

			/* This metadata tells the module the user is delayed join on at least one (or more) channels.
			 * It is only cleared when the user is no longer on ANY +D channels.
			 */
			if (!user->GetExt("delayjoin"))
				user->Extend("delayjoin");
		}
	}

	void OnUserPart(User* user, Channel* channel, const std::string &partmessage, bool &silent)
	{
		if (channel->IsModeSet('D'))
		{
			if (user->GetExt(std::string("delayjoin_")+channel->name))
			{
				silent = true;
				/* Because we silenced the event, make sure it reaches the user whos leaving (but only them of course) */
				user->WriteFrom(user, "PART %s%s%s", channel->name, partmessage.empty() ? "" : " :", partmessage.empty() ? "" : partmessage.c_str());
			}
		}
	}

	void OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent)
	{
		if (chan->IsModeSet('D'))
		{
			/* Send silenced event only to the user being kicked and the user doing the kick */
			if (user->GetExt(std::string("delayjoin_")+chan->name))
			{
				silent = true;
				user->WriteFrom(source, "KICK %s %s %s", chan->name, user->nick, reason.c_str());
			}
		}
	}

	void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message)
	{
		Command* parthandler = ServerInstance->Parser->GetHandler("PART");
		const char* parameters[2];
		if (parthandler && user->GetExt("delayjoin"))
		{
			for (UCListIter f = user->chans.begin(); f != user->chans.end(); f++)
			{
				parameters[0] = f->first->name;
				/* This triggers our OnUserPart, above, making the PART silent */
				parthandler->Handle(parameters, 1, user);
			}
		}
	}

	void OnText(User* user, void* dest, int target_type, const std::string &text, char status, CUList &exempt_list)
	{
		if (target_type != TYPE_CHANNEL)
			return;

		Channel* channel = (Channel*) dest;

		if (!user->GetExt(std::string("delayjoin_")+channel->name))
			return;

		/* Display the join to everyone else (the user who joined got it earlier) */
		channel->WriteAllExcept(user, false, 0, exempt_list, "JOIN %s", channel->name);

		/* Shrink off the neccessary metadata for a specific channel */
		user->Shrink(std::string("delayjoin_")+channel->name);

		/* Check if the user is left on any other +D channels, if so don't take away the
		 * metadata that says theyre on one or more channels 
		 */
		for (UCListIter f = user->chans.begin(); f != user->chans.end(); f++)
			if (f->first->IsModeSet('D'))
				return;

		user->Shrink("delayjoin");
	}
};

MODULE_INIT(ModuleDelayJoin)

