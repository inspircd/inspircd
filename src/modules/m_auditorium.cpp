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

/* $ModDesc: Allows for auditorium channels (+u) where nobody can see others joining and parting or the nick list */

class AuditoriumMode : public ModeHandler
{
 public:
	AuditoriumMode(InspIRCd* Instance) : ModeHandler(Instance, 'u', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (channel->IsModeSet('u') != adding)
		{
			if (IS_LOCAL(source) && (channel->GetStatus(source) < STATUS_OP))
			{
				source->WriteServ("482 %s %s :Only channel operators may %sset channel mode +u", source->nick, channel->name, adding ? "" : "un");
				return MODEACTION_DENY;
			}
			else
			{
				channel->SetMode('u', adding);
				return MODEACTION_ALLOW;
			}
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
	AuditoriumMode* aum;
	bool ShowOps;
	CUList nl;
	CUList except_list;
 public:
	ModuleAuditorium(InspIRCd* Me)
		: Module(Me)
	{
		aum = new AuditoriumMode(ServerInstance);
		if (!ServerInstance->AddMode(aum))
		{
			delete aum;
			throw ModuleException("Could not add new modes!");
		}

		OnRehash(NULL, "");

		Implementation eventlist[] = { I_OnUserJoin, I_OnUserPart, I_OnUserKick, I_OnUserQuit, I_OnUserList, I_OnRehash };
		Me->Modules->Attach(eventlist, this, 6);

	}
	
	virtual ~ModuleAuditorium()
	{
		ServerInstance->Modes->DelMode(aum);
		delete aum;
	}

	void Prioritize()
	{
		Module* namesx = ServerInstance->Modules->Find("m_namesx.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserList, PRIO_BEFORE, &namesx);
	}

	virtual void OnRehash(User* user, const std::string &parameter)
	{
		ConfigReader conf(ServerInstance);
		ShowOps = conf.ReadFlag("auditorium", "showops", 0);
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}

	virtual int OnUserList(User* user, Channel* Ptr, CUList* &nameslist)
	{
		if (Ptr->IsModeSet('u'))
		{
			if (ShowOps)
			{
				/* Leave the names list alone, theyre an op
				 * doing /names on the channel after joining it
				 */
				if (Ptr->GetStatus(user) >= STATUS_OP)
				{
					nameslist = Ptr->GetUsers();
					return 0;
				}

				/* Show all the opped users */
				nl = *(Ptr->GetOppedUsers());
				nl[user] = user->nick;
				nameslist = &nl;
				return 0;
			}
			else
			{
				/* HELLOOO, IS ANYBODY THERE? -- nope, just us. */
				user->WriteServ("353 %s %c %s :%s", user->nick, Ptr->IsModeSet('s') ? '@' : Ptr->IsModeSet('p') ? '*' : '=', Ptr->name, user->nick);
				user->WriteServ("366 %s %s :End of /NAMES list.", user->nick, Ptr->name);
				return 1;
			}
		}
		return 0;
	}
	
	virtual void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent)
	{
		if (channel->IsModeSet('u'))
		{
			silent = true;
			/* Because we silenced the event, make sure it reaches the user whos joining (but only them of course) */
			user->WriteFrom(user, "JOIN %s", channel->name);
			if (ShowOps)
				channel->WriteAllExcept(user, false, channel->GetStatus(user) >= STATUS_OP ? 0 : '@', except_list, "JOIN %s", channel->name);
		}
	}

	void OnUserPart(User* user, Channel* channel, const std::string &partmessage, bool &silent)
	{
		if (channel->IsModeSet('u'))
		{
			silent = true;
			/* Because we silenced the event, make sure it reaches the user whos leaving (but only them of course) */
			user->WriteFrom(user, "PART %s%s%s", channel->name,
					partmessage.empty() ? "" : " :",
					partmessage.empty() ? "" : partmessage.c_str());
			if (ShowOps)
			{
				channel->WriteAllExcept(user, false, channel->GetStatus(user) >= STATUS_OP ? 0 : '@', except_list, "PART %s%s%s", channel->name, partmessage.empty() ? "" : " :",
						partmessage.empty() ? "" : partmessage.c_str());
			}
		}
	}

	void OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent)
	{
		if (chan->IsModeSet('u'))
		{
			silent = true;
			/* Send silenced event only to the user being kicked and the user doing the kick */
			source->WriteFrom(source, "KICK %s %s %s", chan->name, user->nick, reason.c_str());
			if (ShowOps)
				chan->WriteAllExcept(source, false, chan->GetStatus(source) >= STATUS_OP ? 0 : '@', except_list, "KICK %s %s %s", chan->name, user->nick, reason.c_str());
			else
				user->WriteFrom(source, "KICK %s %s %s", chan->name, user->nick, reason.c_str());
		}
	}

	void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message)
	{
		Command* parthandler = ServerInstance->Parser->GetHandler("PART");
		std::vector<std::string> to_leave;
		const char* parameters[2];
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
				parameters[0] = n->c_str();
				/* This triggers our OnUserPart, above, making the PART silent */
				parthandler->Handle(parameters, 1, user);
			}
		}
	}
};

MODULE_INIT(ModuleAuditorium)
