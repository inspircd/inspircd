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
#include "users.h"
#include "channels.h"
#include "modules.h"
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

	virtual bool VisibleTo(userrec* user)
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
		for (user_hash::iterator i = ServerInstance->clientlist->begin(); i != ServerInstance->clientlist->end(); i++)
			if (i->second->Visibility == qo)
				i->second->Visibility = NULL;
		delete qo;
	}

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (source != dest)
			return MODEACTION_DENY;

		if (dest->IsModeSet('Q') != adding)
		{
			bool ok = false;

			for (int j = 0; j < conf->Enumerate("type"); j++)
			{
				std::string opertype = conf->ReadValue("type","name",j);
				if (opertype == source->oper)
				{
					ok = conf->ReadFlag("type", "canquiet", j);
					break;
				}
			}

			if (!ok)
			{
				source->WriteServ("481 %s :Permission Denied - You do not have access to become invisible via user mode +Q", source->nick);
				return MODEACTION_DENY;
			}

			dest->SetMode('Q', adding);

			/* Set visibility handler object */
			dest->Visibility = adding ? qo : NULL;

			/* User appears to vanish or appear from nowhere */
			for (UCListIter f = dest->chans.begin(); f != dest->chans.end(); f++)
			{
				CUList *ulist = f->first->GetUsers();
				char tb[MAXBUF];

				snprintf(tb,MAXBUF,":%s %s %s", dest->GetFullHost(), adding ? "PART" : "JOIN", f->first->name);
				std::string out = tb;
				std::string n = this->ServerInstance->Modes->ModeString(dest, f->first);

				for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
				{
					/* User only appears to vanish for non-opers */
					if (IS_LOCAL(i->first) && !IS_OPER(i->first))
					{
						i->first->Write(out);
						if (!n.empty() && !adding)
							i->first->WriteServ("MODE %s +%s", f->first->name, n.c_str());
					}
				}

				ServerInstance->WriteOpers("*** \2NOTICE\2: Oper %s has become %svisible (%sQ)", dest->GetFullHost(), adding ? "in" : "", adding ? "+" : "-");
			}
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

	bool BeforeMode(userrec* source, userrec* dest, chanrec* channel, std::string &param, bool adding, ModeType type)
	{
		/* Users who are opers and have +Q get their +Q removed when they deoper */
		if ((!adding) && (dest->IsModeSet('Q')))
		{
			const char* newmodes[] = { dest->nick, "-Q" };
			ServerInstance->Modes->Process(newmodes, 2, source, true);
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
		if (!ServerInstance->AddMode(qm, 'Q'))
			throw ModuleException("Could not add new modes!");
		ido = new InvisibleDeOper(ServerInstance);
		if (!ServerInstance->AddModeWatcher(ido))
			throw ModuleException("Could not add new mode watcher on usermode +o!");
	}

	virtual ~ModuleInvisible()
	{
		ServerInstance->Modes->DelMode(qm);
		ServerInstance->Modes->DelModeWatcher(ido);
		DELETE(qm);
		DELETE(ido);
		DELETE(conf);
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = List[I_OnUserJoin] = List[I_OnUserPart] = List[I_OnUserQuit] = List[I_OnRehash] = 1;
	}
	
	virtual void OnUserJoin(userrec* user, chanrec* channel, bool &silent)
	{
		if (user->IsModeSet('Q'))
		{
			silent = true;
			/* Because we silenced the event, make sure it reaches the user whos joining (but only them of course) */
			this->WriteCommonFrom(user, channel, "JOIN %s", channel->name);
			ServerInstance->WriteOpers("*** \2NOTICE\2: Oper %s has joined %s invisibly (+Q)", user->GetFullHost(), channel->name);
		}
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		DELETE(conf);
		conf = new ConfigReader(ServerInstance);
	}

	void OnUserPart(userrec* user, chanrec* channel, const std::string &partmessage, bool &silent)
	{
		if (user->IsModeSet('Q'))
		{
			silent = true;
			/* Because we silenced the event, make sure it reaches the user whos leaving (but only them of course) */
			this->WriteCommonFrom(user, channel, "PART %s%s%s", channel->name,
					partmessage.empty() ? "" : " :",
					partmessage.empty() ? "" : partmessage.c_str());
		}
	}

	void OnUserQuit(userrec* user, const std::string &reason, const std::string &oper_message)
	{
		if (user->IsModeSet('Q'))
		{
			command_t* parthandler = ServerInstance->Parser->GetHandler("PART");
			std::vector<std::string> to_leave;
			const char* parameters[2];
			if (parthandler)
			{
				for (UCListIter f = user->chans.begin(); f != user->chans.end(); f++)
						to_leave.push_back(f->first->name);
				/* We cant do this neatly in one loop, as we are modifying the map we are iterating */
				for (std::vector<std::string>::iterator n = to_leave.begin(); n != to_leave.end(); n++)
				{
					parameters[0] = n->c_str();
					/* This triggers our OnUserPart, above, making the PART silent */
					parthandler->Handle(parameters, 1, user);
				}
			}
		}
	}

	/* No privmsg response when hiding - submitted by Eric at neowin */
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_USER) && (IS_LOCAL(user)))
		{
			userrec* target = (userrec*)dest;
			if(target->IsModeSet('Q') && !*user->oper)
			{
				user->WriteServ("401 %s %s :No such nick/channel",user->nick, target->nick);
				return 1;
			}
		}
		return 0;
	}
	
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreNotice(user, dest, target_type, text, status, exempt_list);
	}

	/* Fix by Eric @ neowin.net, thanks :) -- Brain */
	void WriteCommonFrom(userrec *user, chanrec* channel, const char* text, ...)
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
			/* User only appears to vanish for non-opers */
			if (IS_LOCAL(i->first) && IS_OPER(i->first))
			{
				i->first->Write(std::string(tb));
			}
		}
	}

};

MODULE_INIT(ModuleInvisible)
