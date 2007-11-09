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

/* $ModDesc: Provides support for unreal-style CYCLE command. */

/** Handle /CYCLE
 */
class CommandCycle : public Command
{
 public:
	CommandCycle (InspIRCd* Instance) : Command(Instance,"CYCLE", 0, 1, false, 3)
	{
		this->source = "m_cycle.so";
		syntax = "<channel> :[reason]";
		TRANSLATE3(TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		Channel* channel = ServerInstance->FindChan(parameters[0]);
		std::string reason = ConvToStr("Cycling");
			
		if (pcnt > 1)
		{
			/* reason provided, use it */
			reason = reason + ": " + parameters[1];
		}
		
		if (!channel)
		{
			user->WriteServ("403 %s %s :No such channel", user->nick, parameters[0]);
			return CMD_FAILURE;
		}
		
		if (channel->HasUser(user))
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
				
				/* XXX in the future, this may move to a static Channel method (the delete.) -- w00t */
				if (!channel->PartUser(user, reason.c_str()))
					delete channel;
				
				Channel::JoinUser(ServerInstance, user, parameters[0], true, "", false, ServerInstance->Time(true));
			}

			return CMD_LOCALONLY;
		}
		else
		{
			user->WriteServ("442 %s %s :You're not on that channel", user->nick, channel->name);
		}

		return CMD_FAILURE;
	}
};


class ModuleCycle : public Module
{
	CommandCycle*	mycommand;
 public:
	ModuleCycle(InspIRCd* Me)
		: Module(Me)
	{
		
		mycommand = new CommandCycle(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}
	
	virtual ~ModuleCycle()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 1, VF_COMMON | VF_VENDOR, API_VERSION);
	}
	
};

MODULE_INIT(ModuleCycle)
