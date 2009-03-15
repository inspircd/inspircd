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
#include <stdarg.h>

/* $ModDesc: Allows for opered clients to join channels without being seen, similar to unreal 3.1 +I mode */

static ConfigReader* conf;

class QuietOper : public VisData
{
 public:
	QuietOper()
	{
	}

	virtual ~QuietOper()
	{
	}

	virtual bool VisibleTo(User* user)
	{
		return IS_OPER(user);
	}
};


class InvisibleMode : public ModeHandler
{
	QuietOper* qo;
 public:
	InvisibleMode(InspIRCd* Instance) : ModeHandler(Instance, 'Q', 0, 0, false, MODETYPE_USER, true)
	{
		qo = new QuietOper();
	}

	~InvisibleMode()
	{
		for (user_hash::iterator i = ServerInstance->Users->clientlist->begin(); i != ServerInstance->Users->clientlist->end(); i++)
			if (i->second->Visibility == qo)
				i->second->Visibility = NULL;
		delete qo;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		if (dest->IsModeSet('Q') != adding)
		{
			dest->SetMode('Q', adding);

			/* Fix for bug #379 reported by stealth. On +/-Q make m_watch think the user has signed on/off */
			Module* m = ServerInstance->Modules->Find("m_watch.so");

			/* This must come before setting/unsetting the handler */
			if (m && adding)
				m->OnUserQuit(dest, "Connection closed", "Connection closed");

			/* Set visibility handler object */
			dest->Visibility = adding ? qo : NULL;

			/* This has to come after setting/unsetting the handler */
			if (m && !adding)
				m->OnPostConnect(dest);

			/* User appears to vanish or appear from nowhere */
			for (UCListIter f = dest->chans.begin(); f != dest->chans.end(); f++)
			{
				CUList *ulist = f->first->GetUsers();
				char tb[MAXBUF];

				snprintf(tb,MAXBUF,":%s %s %s", dest->GetFullHost().c_str(), adding ? "PART" : "JOIN", f->first->name.c_str());
				std::string out = tb;
				std::string n = this->ServerInstance->Modes->ModeString(dest, f->first);

				for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
				{
					/* User only appears to vanish for non-opers */
					if (IS_LOCAL(i->first) && !IS_OPER(i->first))
					{
						i->first->Write(out);
						if (!n.empty() && !adding)
							i->first->WriteServ("MODE %s +%s", f->first->name.c_str(), n.c_str());
					}
				}
			}

			ServerInstance->SNO->WriteToSnoMask('A', "\2NOTICE\2: Oper %s has become %svisible (%cQ)", dest->GetFullHost().c_str(), adding ? "in" : "", adding ? '+' : '-');
			return MODEACTION_ALLOW;
		}
		else
		{
			return MODEACTION_DENY;
		}
	}
};

class InvisibleDeOper : public ModeWatcher
{
 private:
	InspIRCd* Srv;
 public:
	InvisibleDeOper(InspIRCd* Instance) : ModeWatcher(Instance, 'o', MODETYPE_USER), Srv(Instance)
	{
	}

	bool BeforeMode(User* source, User* dest, Channel* channel, std::string &param, bool adding, ModeType type, bool)
	{
		/* Users who are opers and have +Q get their +Q removed when they deoper */
		if ((!adding) && (dest->IsModeSet('Q')))
		{
			std::vector<std::string> newmodes;
			newmodes.push_back(dest->nick);
			newmodes.push_back("-Q");
			ServerInstance->Modes->Process(newmodes, source, true);
		}
		return true;
	}
};


class ModuleInvisible : public Module
{
 private:
	InvisibleMode* qm;
	InvisibleDeOper* ido;
 public:
	ModuleInvisible(InspIRCd* Me)
		: Module(Me)
	{
		conf = new ConfigReader(ServerInstance);
		qm = new InvisibleMode(ServerInstance);
		if (!ServerInstance->Modes->AddMode(qm))
			throw ModuleException("Could not add new modes!");
		ido = new InvisibleDeOper(ServerInstance);
		if (!ServerInstance->Modes->AddModeWatcher(ido))
			throw ModuleException("Could not add new mode watcher on usermode +o!");

		/* Yeah i know people can take this out. I'm not about to obfuscate code just to be a pain in the ass. */
		ServerInstance->Users->ServerNoticeAll("*** m_invisible.so has just been loaded on this network. For more information, please visit http://inspircd.org/wiki/Modules/invisible");
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_OnUserJoin, I_OnUserPart, I_OnUserQuit, I_OnRehash, I_OnHostCycle };
		ServerInstance->Modules->Attach(eventlist, this, 7);
	};

	virtual ~ModuleInvisible()
	{
		ServerInstance->Modes->DelMode(qm);
		ServerInstance->Modes->DelModeWatcher(ido);
		delete qm;
		delete ido;
		delete conf;
	};

	virtual Version GetVersion();
	virtual void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent);
	virtual void OnRehash(User* user, const std::string &parameter);
	void OnUserPart(User* user, Channel* channel, std::string &partmessage, bool &silent);
	void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message);
	bool OnHostCycle(User* user);
	/* No privmsg response when hiding - submitted by Eric at neowin */
	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list);
	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list);
	/* Fix by Eric @ neowin.net, thanks :) -- Brain */
	void WriteCommonFrom(User *user, Channel* channel, const char* text, ...) CUSTOM_PRINTF(4, 5);
};

Version ModuleInvisible::GetVersion()
{
	return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
}

void ModuleInvisible::OnUserJoin(User* user, Channel* channel, bool sync, bool &silent)
{
	if (user->IsModeSet('Q'))
	{
		silent = true;
		/* Because we silenced the event, make sure it reaches the user whos joining (but only them of course) */
		this->WriteCommonFrom(user, channel, "JOIN %s", channel->name.c_str());
		ServerInstance->SNO->WriteToSnoMask('A', "\2NOTICE\2: Oper %s has joined %s invisibly (+Q)", user->GetFullHost().c_str(), channel->name.c_str());
	}
}

void ModuleInvisible::OnRehash(User* user, const std::string &parameter)
{
	delete conf;
	conf = new ConfigReader(ServerInstance);
}

void ModuleInvisible::OnUserPart(User* user, Channel* channel, std::string &partmessage, bool &silent)
{
	if (user->IsModeSet('Q'))
	{
		silent = true;
		/* Because we silenced the event, make sure it reaches the user whos leaving (but only them of course) */
		this->WriteCommonFrom(user, channel, "PART %s%s%s", channel->name.c_str(),
				partmessage.empty() ? "" : " :",
				partmessage.empty() ? "" : partmessage.c_str());
	}
}

void ModuleInvisible::OnUserQuit(User* user, const std::string &reason, const std::string &oper_message)
{
	if (user->IsModeSet('Q'))
	{
		Command* parthandler = ServerInstance->Parser->GetHandler("PART");
		std::vector<std::string> to_leave;
		if (parthandler)
		{
			for (UCListIter f = user->chans.begin(); f != user->chans.end(); f++)
					to_leave.push_back(f->first->name);
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
}

bool ModuleInvisible::OnHostCycle(User* user)
{
	return user->IsModeSet('Q');
}

/* No privmsg response when hiding - submitted by Eric at neowin */
int ModuleInvisible::OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
{
	if ((target_type == TYPE_USER) && (IS_LOCAL(user)))
	{
		User* target = (User*)dest;
		if(target->IsModeSet('Q') && !IS_OPER(user))
		{
			user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), target->nick.c_str());
			return 1;
		}
	}
	return 0;
}

int ModuleInvisible::OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
{
	return OnUserPreNotice(user, dest, target_type, text, status, exempt_list);
}

/* Fix by Eric @ neowin.net, thanks :) -- Brain */
void ModuleInvisible::WriteCommonFrom(User *user, Channel* channel, const char* text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];
	char tb[MAXBUF];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);
	snprintf(tb,MAXBUF,":%s %s",user->GetFullHost().c_str(), textbuffer);

	CUList *ulist = channel->GetUsers();

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		/* User only appears to vanish for non-opers */
		if (IS_LOCAL(i->first) && IS_OPER(i->first))
		{
			i->first->Write(std::string(tb));
		}
	}
}

MODULE_INIT(ModuleInvisible)
