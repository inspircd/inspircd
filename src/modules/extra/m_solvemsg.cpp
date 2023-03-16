/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015-2016 Sadie Powell <sadie@witchery.services>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModConfig: <solvemsg chanmsg="no" usermsg="yes">
/// $ModDepends: core 3
/// $ModDesc: Requires users to solve a basic maths problem before messaging others.


#include "inspircd.h"
#include "modules/account.h"

struct Problem
{
	int first;
	int second;
	time_t nextwarning;
};

class CommandSolve : public SplitCommand
{
 private:
	SimpleExtItem<Problem>& ext;

 public:
	CommandSolve(Module* Creator, SimpleExtItem<Problem>& Ext)
		: SplitCommand(Creator, "SOLVE", 1, 1)
		, ext(Ext)
	{
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE
	{
		Problem* problem = ext.get(user);
		if (!problem)
		{
			user->WriteNotice("** You have already solved your problem!");
			return CMD_FAILURE;
		}

		int result = ConvToNum<int>(parameters[0]);
		if (result != (problem->first + problem->second))
		{
			user->WriteNotice(InspIRCd::Format("*** %s is not the correct answer.", parameters[0].c_str()));
			user->CommandFloodPenalty += 10000;
			return CMD_FAILURE;
		}

		ext.unset(user);
		user->WriteNotice(InspIRCd::Format("*** %s is the correct answer!", parameters[0].c_str()));
		return CMD_SUCCESS;
	}
};

class ModuleSolveMessage : public Module
{
 private:
	SimpleExtItem<Problem> ext;
	CommandSolve cmd;
	bool chanmsg;
	bool usermsg;
	bool exemptregistered;
	time_t warntime;

 public:
	ModuleSolveMessage()
		: ext("solve-message", ExtensionItem::EXT_USER, this)
		, cmd(this, ext)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("solvemsg");
		chanmsg = tag->getBool("chanmsg", false);
		usermsg = tag->getBool("usermsg", true);
		exemptregistered = tag->getBool("exemptregistered", true);
		warntime = tag->getDuration("warntime", 60, 1);
	}

	void OnUserPostInit(LocalUser* user) CXX11_OVERRIDE
	{
		Problem problem;
		problem.first = ServerInstance->GenRandomInt(9);
		problem.second = ServerInstance->GenRandomInt(9);
		problem.nextwarning = 0;
		ext.set(user, problem);
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& msgtarget, MessageDetails& details) CXX11_OVERRIDE
	{
		LocalUser* source = IS_LOCAL(user);
		if (!source)
			return MOD_RES_PASSTHRU;

		if (!source->MyClass->config->getBool("usesolvemsg", true))
			return MOD_RES_PASSTHRU; // Exempt by connect class.

		if (exemptregistered)
		{
			const AccountExtItem* accextitem = GetAccountExtItem();
			const std::string* account = accextitem ? accextitem->get(user) : NULL;
			if (account)
				return MOD_RES_PASSTHRU; // Exempt logged in users.
		}

		switch (msgtarget.type)
		{
			case MessageTarget::TYPE_USER:
			{
				if (!usermsg)
					return MOD_RES_PASSTHRU; // Not enabled.

				User* target = msgtarget.Get<User>();
				if (target->server->IsULine())
					return MOD_RES_PASSTHRU; // Allow messaging ulines.

				break;
			}

			case MessageTarget::TYPE_CHANNEL:
			{
				if (!chanmsg)
					return MOD_RES_PASSTHRU; // Not enabled.

				Channel* target = msgtarget.Get<Channel>();
				if (target->GetPrefixValue(user) >= VOICE_VALUE)
					return MOD_RES_PASSTHRU; // Exempt users with a status rank.
				break;
			}

			case MessageTarget::TYPE_SERVER:
				return MOD_RES_PASSTHRU; // Only opers can do this.
		}

		Problem* problem = ext.get(user);
		if (!problem)
			return MOD_RES_PASSTHRU;

		if (problem->nextwarning > ServerInstance->Time())
			return MOD_RES_DENY;

		user->WriteNotice("*** Before you can send messages you must solve the following problem:");
		user->WriteNotice(InspIRCd::Format("*** What is %d + %d?", problem->first, problem->second));
		user->WriteNotice("*** You can enter your answer using /QUOTE SOLVE <answer>");
		problem->nextwarning = ServerInstance->Time() + warntime;
		return MOD_RES_DENY;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Requires users to solve a basic maths problem before messaging others.");
	}
};

MODULE_INIT(ModuleSolveMessage)
