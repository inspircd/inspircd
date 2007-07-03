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
#include "hashcomp.h"

/* $ModDesc: Provides the NICKLOCK command, allows an oper to chage a users nick and lock them to it until they quit */

/** Handle /NICKLOCK
 */
class cmd_nicklock : public command_t
{
	char* dummy;
 public:
 cmd_nicklock (InspIRCd* Instance) : command_t(Instance,"NICKLOCK", 'o', 2)
	{
		this->source = "m_nicklock.so";
		syntax = "<oldnick> <newnick>";
	}

	CmdResult Handle(const char** parameters, int pcnt, userrec *user)
	{
		userrec* source = ServerInstance->FindNick(parameters[0]);
		irc::string server;
		irc::string me;

		// check user exists
		if (!source)
		{
			return CMD_FAILURE;
		}

		// check if user is locked
		if (source->GetExt("nick_locked", dummy))
		{
			user->WriteServ("946 %s %s :This user's nickname is already locked.",user->nick,source->nick);
			return CMD_FAILURE;
		}

		// check nick is valid
		if (!ServerInstance->IsNick(parameters[1]))
		{
			return CMD_FAILURE;
		}

		// let others know
		ServerInstance->WriteOpers(std::string(user->nick)+" used NICKLOCK to change and hold "+parameters[0]+" to "+parameters[1]);

		if (!source->ForceNickChange(parameters[1]))
		{
			// ugh, nickchange failed for some reason -- possibly existing nick?
			userrec::QuitUser(ServerInstance, source, "Nickname collision");
			return CMD_FAILURE;
		}

		// give them a lock flag
		source->Extend("nick_locked", "ON");

		/* route */
		return CMD_SUCCESS;
	}
};

/** Handle /NICKUNLOCK
 */
class cmd_nickunlock : public command_t
{
 public:
 cmd_nickunlock (InspIRCd* Instance) : command_t(Instance,"NICKUNLOCK", 'o', 1)
	{
		this->source = "m_nickunlock.so";
		syntax = "<locked-nick>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* source = ServerInstance->FindNick(parameters[0]);
		if (source)
		{
			source->Shrink("nick_locked");
			user->WriteServ("945 %s %s :Nickname now unlocked.",user->nick,source->nick);
			ServerInstance->WriteOpers(std::string(user->nick)+" used NICKUNLOCK on "+parameters[0]);
			return CMD_SUCCESS;
		}

		return CMD_FAILURE;
	}
};


class ModuleNickLock : public Module
{
	cmd_nicklock*	cmd1;
	cmd_nickunlock*	cmd2;
	char* n;
 public:
	ModuleNickLock(InspIRCd* Me)
		: Module(Me)
	{
		
		cmd1 = new cmd_nicklock(ServerInstance);
		cmd2 = new cmd_nickunlock(ServerInstance);
		ServerInstance->AddCommand(cmd1);
		ServerInstance->AddCommand(cmd2);
	}
	
	virtual ~ModuleNickLock()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnUserPreNick] = List[I_OnUserQuit] = List[I_OnCleanup] = 1;
	}

	virtual int OnUserPreNick(userrec* user, const std::string &newnick)
	{
		if (user->GetExt("nick_locked", n))
		{
			user->WriteServ("447 %s :You cannot change your nickname (your nick is locked)",user->nick);
			return 1;
		}
		return 0;
	}

	virtual void OnUserQuit(userrec* user, const std::string &reason, const std::string &oper_message)
	{
		user->Shrink("nick_locked");
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			userrec* user = (userrec*)item;
			user->Shrink("nick_locked");
		}
	}
};

MODULE_INIT(ModuleNickLock)
