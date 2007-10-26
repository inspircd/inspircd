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
	CUList except_list;
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
		List[I_OnUserJoin] = List[I_OnUserPart] = List[I_OnUserKick] = List[I_OnUserQuit] = List[I_OnUserList] = 
		List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = 1;
	}

	virtual int OnUserList(User* user, Channel* Ptr, CUList* &nameslist)
	{
		if (Ptr->IsModeSet('D'))
		{
			nl = *nameslist;

			for (CUListIter n = nameslist->begin(); n != nameslist->end(); ++n)
			{
				if (n->first->GetExt("delayjoin_notspoken"))
					nl.erase(n->first);
			}

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
			user->Extend(std::string("delayjoin_")+channel->name);
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
		std::vector<std::string> to_leave;
		const char* parameters[2];
		if (parthandler && user->GetExt("delayjoin"))
		{
			for (UCListIter f = user->chans.begin(); f != user->chans.end(); f++)
			{
				if (f->first->IsModeSet('D'))
					to_leave.push_back(f->first->name);
			}
			/* We cant do this neatly in one loop, as we are modifying the map we are iterating */
			for (std::vector<std::string>::iterator n = to_leave.begin(); n != to_leave.end(); n++)
			{
				parameters[0] = n->c_str();
				/* This triggers our OnUserPart, above, making the PART silent */
				parthandler->Handle(parameters, 1, user);
			}
		}
	}
};

MODULE_INIT(ModuleDelayJoin)

