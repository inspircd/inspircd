/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <stdarg.h>

/* $ModDesc: Allows for delay-join channels (+D) where users dont appear to join until they speak */

class DelayJoinMode : public ModeHandler
{
	CUList empty;
	Module* Creator;
 public:
	DelayJoinMode(InspIRCd* Instance, Module* Parent) : ModeHandler(Instance, 'D', 0, 0, false, MODETYPE_CHANNEL, false), Creator(Parent) { }

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
				if (channel->IsModeSet('D'))
				{
					/* Make all delayed join users visible, or if an op removes +D
					 * while users exist that havent spoken, they remain permenantly
					 * invisible on this channel!
					 */
					CUList* names = channel->GetUsers();
					for (CUListIter n = names->begin(); n != names->end(); ++n)
						Creator->OnText(n->first, channel, TYPE_CHANNEL, "", 0, empty);
				}
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
		djm = new DelayJoinMode(ServerInstance, this);
		if (!ServerInstance->AddMode(djm))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserJoin, I_OnUserPart, I_OnUserKick, I_OnUserQuit, I_OnUserList, I_OnText };
		ServerInstance->Modules->Attach(eventlist, this, 6);
	}
	
	virtual ~ModuleDelayJoin()
	{
		ServerInstance->Modes->DelMode(djm);
		delete djm;
	}

	void Prioritize()
	{
		/* To ensure that we get priority over namesx for names list generation */
		Module* namesx = ServerInstance->Modules->Find("m_namesx.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserList, PRIO_BEFORE, &namesx);
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
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

			/* Modify the names list, erasing users with the delay join metadata
			 * for this channel (havent spoken yet)
			 */
			for (CUListIter n = newlist->begin(); n != newlist->end(); ++n)
			{
				if (!n->first->GetExt(key))
					nl.insert(*n);
			}

			/* Always show self */
			nl[user] = user->nick;
			nameslist = &nl;
		}
		return 0;
	}

	virtual void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent)
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
		this->WriteCommonFrom(user, channel, "JOIN %s", channel->name);

		std::string n = this->ServerInstance->Modes->ModeString(user, channel);
		if (n.length() > 0)
			this->WriteCommonFrom(user, channel, "MODE %s +%s", channel->name, n.c_str());

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

	void WriteCommonFrom(User *user, Channel* channel, const char* text, ...)
	{
		va_list argsPtr;
		char textbuffer[MAXBUF];
		char tb[MAXBUF];

		va_start(argsPtr, text);
		vsnprintf(textbuffer, MAXBUF, text, argsPtr);
		va_end(argsPtr);
		snprintf(tb,MAXBUF,":%s %s",user->GetFullHost(),textbuffer);

		CUList *ulist = channel->GetUsers();

		for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			/* User doesnt get a JOIN sent to themselves */
			if (user == i->first)
				continue;

			/* Users with a visibility state that hides them dont appear */
			if (user->Visibility && !user->Visibility->VisibleTo(i->first))
				continue;

			i->first->Write(std::string(tb));
		}
	}

};

MODULE_INIT(ModuleDelayJoin)

