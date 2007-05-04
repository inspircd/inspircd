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

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

/* $ModDesc: Allows for auditorium channels (+u) where nobody can see others joining and parting or the nick list */

class AuditoriumMode : public ModeHandler
{
 public:
	AuditoriumMode(InspIRCd* Instance) : ModeHandler(Instance, 'u', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
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
 public:
	ModuleAuditorium(InspIRCd* Me)
		: Module::Module(Me)
	{
		aum = new AuditoriumMode(ServerInstance);
		if (!ServerInstance->AddMode(aum, 'u'))
			throw ModuleException("Could not add new modes!");
	}
	
	virtual ~ModuleAuditorium()
	{
		ServerInstance->Modes->DelMode(aum);
		DELETE(aum);
	}

	Priority Prioritize()
	{
		/* To ensure that we get priority over namesx for names list generation on +u channels */
		return (Priority)ServerInstance->PriorityBefore("m_namesx.so");
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnUserJoin] = List[I_OnUserPart] = List[I_OnUserKick] = List[I_OnUserQuit] = List[I_OnUserList] = 1;
	}

	virtual int OnUserList(userrec* user, chanrec* Ptr)
	{
		if (Ptr->IsModeSet('u'))
		{
			/* HELLOOO, IS ANYBODY THERE? -- nope, just us. */
			user->WriteServ("353 %s = %s :%s", user->nick, Ptr->name, user->nick);
			user->WriteServ("366 %s %s :End of /NAMES list.", user->nick, Ptr->name);
			return 1;
		}
		return 0;
	}
	
	virtual void OnUserJoin(userrec* user, chanrec* channel, bool &silent)
	{
		if (channel->IsModeSet('u'))
			silent = true;
	}

	void OnUserPart(userrec* user, chanrec* channel, const std::string &partmessage, bool &silent)
	{
		if (channel->IsModeSet('u'))
			silent = true;
	}

	void OnUserKick(userrec* source, userrec* user, chanrec* chan, const std::string &reason, bool &silent)
	{
		if (chan->IsModeSet('u'))
			silent = true;
	}

	void OnUserQuit(userrec* user, const std::string &reason, const std::string &oper_message)
	{
		command_t* parthandler = ServerInstance->Parser->GetHandler("PART");
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

class ModuleAuditoriumFactory : public ModuleFactory
{
 public:
	ModuleAuditoriumFactory()
	{
	}
	
	~ModuleAuditoriumFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleAuditorium(Me);
	}
	
};

extern "C" void * init_module( void )
{
	return new ModuleAuditoriumFactory;
}

