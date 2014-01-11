/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Adam <Adam@anope.org>
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


#include "inspircd.h"

namespace
{
	LocalStringExt *bncPass;
}

class CommandBNC : public Command
{
 public:
	CommandBNC(Module* Creator) : Command(Creator, "BNC", 0, 1)
	{
		syntax = "[password]";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		if (parameters.empty())
		{
			bncPass->unset(user);
			user->WriteNotice("*** BNC disabled.");
		}
		else
		{
			const std::string &password = parameters[0].substr(0, 32);
			user->WriteNotice("*** Set your server password to " + user->nick + ":" + password + " to reattach.");
			bncPass->set(user, password);
		}

		return CMD_SUCCESS;
	}
};


class ModuleBNC : public Module
{
	CommandBNC cmd;
	LocalStringExt bncPassword;

	bool CanAttach(LocalUser *old, LocalUser *user)
	{
		return user->ehs.size() <= 5; // XXX
	}

	void Attach(LocalUser *old, LocalUser *user)
	{
		/* Called to attach a new client to 'user'. 'old' is the client that is being merged with 'user', it will go away when this function returns.
		 * Any writes to user within this function will only go to the new user, and not to other users that might be attached
		 */

		user->WriteNotice("Welcome back to " + ServerInstance->Config->Network + ", " + user->nick + "!");

		/* Begin code dupe */

		/* Welcome user */
		user->WriteNumeric(RPL_WELCOME, ":Welcome to the %s IRC Network %s", ServerInstance->Config->Network.c_str(), user->GetFullRealHost().c_str());
		user->WriteNumeric(RPL_YOURHOSTIS, ":Your host is %s, running version %s", ServerInstance->Config->ServerName.c_str(), BRANCH);
		user->WriteNumeric(RPL_SERVERCREATED, ":This server was created %s %s", __TIME__, __DATE__);

		const std::string& modelist = ServerInstance->Modes->GetModeListFor004Numeric();
		user->WriteNumeric(RPL_SERVERVERSION, "%s %s %s", ServerInstance->Config->ServerName.c_str(), BRANCH, modelist.c_str());

		ServerInstance->ISupport.SendTo(user);
		user->WriteNumeric(RPL_YOURUUID, "%s :your unique ID", user->uuid.c_str());

		/* Tell user about the channels they're in */
		for (UCListIter i = user->chans.begin(); i != user->chans.end(); i++)
		{
			Channel* c = *i;

			user->WriteFrom(user, "JOIN :" + c->name);

			if (c->topicset)
			{
				user->WriteNumeric(RPL_TOPIC, "%s :%s", c->name.c_str(), c->topic.c_str());
				user->WriteNumeric(RPL_TOPICTIME, "%s %s %lu", c->name.c_str(), c->setby.c_str(), (unsigned long)c->topicset);
			}

			c->UserList(user);
		}

		/* End dupe */
	}

	ModResult HandleQuit(LocalUser *user)
	{
		if (bncPassword.get(user))
		{
			user->Write("ERROR :Closing link: (%s@%s) [%s]", user->ident.c_str(), user->host.c_str(), "Detached");
			user->currentHandler->OnError(I_ERR_DISCONNECT);
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult HandlePass(LocalUser *user, const std::vector<std::string>& parameters)
	{
		if (user->registered != REG_NONE || parameters.empty())
			return MOD_RES_PASSTHRU;

		size_t i = parameters[0].find(':');
		if (i == std::string::npos)
			return MOD_RES_PASSTHRU;

		std::string username = parameters[0].substr(0, i), password = parameters[0].substr(i + 1);

		User *u = ServerInstance->FindNickOnly(username);
		if (u == NULL || !IS_LOCAL(u))
			return MOD_RES_PASSTHRU;
		LocalUser *lu = IS_LOCAL(u);

		std::string *bncpass = bncPassword.get(u);
		if (bncpass == NULL || password != *bncpass)
			return MOD_RES_PASSTHRU;

		if (!CanAttach(user, lu))
			return MOD_RES_PASSTHRU;

		UserIOHandler *io = user->currentHandler;

		/* Detach I/O handler from old client */
		user->currentHandler = NULL;
		user->ehs.clear();

		/* Re-assign user for this I/O handler */
		io->user = lu;

		/* Temporarily copy existing ehs for this user so we may burst to only the new user */
		std::vector<UserIOHandler *> ehs;
		lu->ehs.swap(ehs);

		/* Add new user */
		lu->ehs.push_back(io);

		/* Attach user */
		Attach(user, lu);

		/* Swap back old handlers */
		ehs.swap(lu->ehs);

		/* Add new handler */
		lu->ehs.push_back(io);

		/* Old user goes away */
		ServerInstance->Users->QuitUser(user, "User is attaching to BNC");

		return MOD_RES_DENY;
	}

	ModResult HandleUser(LocalUser *user)
	{
		/* Ignore USER from bnc users, the client will send this right after reattaching */
		return bncPassword.get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}

 public:
	ModuleBNC() : cmd(this), bncPassword("bncPassword", this)
	{
		bncPass = &bncPassword;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("BNC", VF_VENDOR);
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string>& parameters, LocalUser *user, bool validated, const std::string &original_line) CXX11_OVERRIDE
	{
		if (command == "QUIT")
			return HandleQuit(user);

		if (command == "PASS")
			return HandlePass(user, parameters);

		if (command == "USER")
			return HandleUser(user);

		return MOD_RES_PASSTHRU;
	}

	void OnPostCommand(Command* command, const std::vector<std::string>& parameters, LocalUser* user, CmdResult result, const std::string& original_line) CXX11_OVERRIDE
	{
		if (result != CMD_SUCCESS)
			return;

		if (command->name != "NOTICE" && command->name != "PRIVMSG")
			return;

		if (user->ehs.size() <= 1)
			return;

		/* Make privmsgs and notices sent by users show up on their other clients of the user */

		std::vector<UserIOHandler *>::iterator it = std::find(user->ehs.begin(), user->ehs.end(), user->currentHandler);
		if (it == user->ehs.end())
			return;

		user->ehs.erase(it);
		user->WriteFrom(user, command->name + " " + parameters[0] + " :" + parameters[1]);
		user->ehs.push_back(user->currentHandler);
	}

	ModResult OnUserIOError(LocalUser *user) CXX11_OVERRIDE
	{
		return bncPassword.get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}

	ModResult OnPingTimeout(LocalUser *user, UserIOHandler *handler) CXX11_OVERRIDE
	{
		if (!bncPassword.get(user))
			return MOD_RES_PASSTHRU;

		/* 'handler' for 'user' pinged out, so it goes away */
		ServerInstance->GlobalCulls.AddItem(handler);
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleBNC)
