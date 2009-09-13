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

/* $ModDesc: Provides support for unreal-style CYCLE command. */

/** Handle /CYCLE
 */
class CommandCycle : public Command
{
 public:
	CommandCycle (InspIRCd* Instance, Module* Creator) : Command(Instance, Creator,"CYCLE", 0, 1, false, 3)
	{
		syntax = "<channel> :[reason]";
		TRANSLATE3(TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		Channel* channel = ServerInstance->FindChan(parameters[0]);
		std::string reason = ConvToStr("Cycling");

		if (parameters.size() > 1)
		{
			/* reason provided, use it */
			reason = reason + ": " + parameters[1];
		}

		if (!channel)
		{
			user->WriteNumeric(403, "%s %s :No such channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		if (channel->HasUser(user))
		{
			/*
			 * technically, this is only ever sent locally, but pays to be safe ;p
			 */
			if (IS_LOCAL(user))
			{
				if (channel->GetPrefixValue(user) < VOICE_VALUE && channel->IsBanned(user))
				{
					/* banned, boned. drop the message. */
					user->WriteServ("NOTICE "+std::string(user->nick)+" :*** You may not cycle, as you are banned on channel " + channel->name);
					return CMD_FAILURE;
				}

				/* XXX in the future, this may move to a static Channel method (the delete.) -- w00t */
				if (!channel->PartUser(user, reason))
					delete channel;

				Channel::JoinUser(ServerInstance, user, parameters[0].c_str(), true, "", false, ServerInstance->Time());
			}

			return CMD_SUCCESS;
		}
		else
		{
			user->WriteNumeric(442, "%s %s :You're not on that channel", user->nick.c_str(), channel->name.c_str());
		}

		return CMD_FAILURE;
	}
};


class ModuleCycle : public Module
{
	CommandCycle cmd;
 public:
	ModuleCycle(InspIRCd* Me)
		: Module(Me), cmd(Me, this)
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleCycle()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleCycle)
