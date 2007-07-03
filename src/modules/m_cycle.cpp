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

/* $ModDesc: Provides support for unreal-style CYCLE command. */

/** Handle /CYCLE
 */
class cmd_cycle : public command_t
{
 public:
	cmd_cycle (InspIRCd* Instance) : command_t(Instance,"CYCLE", 0, 1)
	{
		this->source = "m_cycle.so";
		syntax = "<channel> :[reason]";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		chanrec* channel = ServerInstance->FindChan(parameters[0]);
		std::string reason = ConvToStr("Cycling");
			
		if (pcnt > 1)
		{
			/* reason provided, use it */
			reason = reason + ": " + parameters[1];
		}
		
		if (channel)
		{
			/*
			 * technically, this is only ever sent locally, but pays to be safe ;p
			 */
			if (IS_LOCAL(user))
			{
				if (channel->GetStatus(user) < STATUS_VOICE && channel->IsBanned(user))
				{
					/* banned, boned. drop the message. */
					user->WriteServ("NOTICE "+std::string(user->nick)+" :*** You may not cycle, as you are banned on channel " + channel->name);
					return CMD_FAILURE;
				}
				
				/* XXX in the future, this may move to a static chanrec method (the delete.) -- w00t */
				if (!channel->PartUser(user, reason.c_str()))
					delete channel;
				
				chanrec::JoinUser(ServerInstance, user, parameters[0], true, "", ServerInstance->Time(true));
			}

			return CMD_LOCALONLY;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** You are not on this channel", user->nick);
		}

		return CMD_FAILURE;
	}
};


class ModuleCycle : public Module
{
	cmd_cycle*	mycommand;
 public:
	ModuleCycle(InspIRCd* Me)
		: Module(Me)
	{
		
		mycommand = new cmd_cycle(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleCycle()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
	
};

MODULE_INIT(ModuleCycle)
