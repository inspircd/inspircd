/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
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
	CommandCycle(Module* Creator) : Command(Creator,"CYCLE", 1)
	{
		Penalty = 3; syntax = "<channel> :[reason]";
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
				if (channel->GetAccessRank(user) < VOICE_VALUE && channel->IsBanned(user))
				{
					/* banned, boned. drop the message. */
					user->WriteServ("NOTICE %s :*** You may not cycle, as you are banned on channel %s", user->nick.c_str(), channel->name.c_str());
					return CMD_FAILURE;
				}

				channel->PartUser(user, reason);

				Channel::JoinUser(user, parameters[0].c_str(), true, "", false, ServerInstance->Time());
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
	ModuleCycle() : cmd(this) {}
	void init()
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleCycle()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for unreal-style CYCLE command.", VF_VENDOR);
	}

};

MODULE_INIT(ModuleCycle)
