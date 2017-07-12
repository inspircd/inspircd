/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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
#include "xline.h"

class CommandClearChan : public Command
{
 public:
	Channel* activechan;

	CommandClearChan(Module* Creator)
		: Command(Creator, "CLEARCHAN", 1, 3)
	{
		syntax = "<channel> [<KILL|KICK|G|Z>] [<reason>]";
		flags_needed = 'o';

		// Stop the linking mod from forwarding ENCAP'd CLEARCHAN commands, see below why
		force_manual_route = true;
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		Channel* chan = activechan = ServerInstance->FindChan(parameters[0]);
		if (!chan)
		{
			user->WriteNotice("The channel " + parameters[0] + " does not exist.");
			return CMD_FAILURE;
		}

		// See what method the oper wants to use, default to KILL
		std::string method("KILL");
		if (parameters.size() > 1)
		{
			method = parameters[1];
			std::transform(method.begin(), method.end(), method.begin(), ::toupper);
		}

		XLineFactory* xlf = NULL;
		bool kick = (method == "KICK");
		if ((!kick) && (method != "KILL"))
		{
			if ((method != "Z") && (method != "G"))
			{
				user->WriteNotice("Invalid method for clearing " + chan->name);
				return CMD_FAILURE;
			}

			xlf = ServerInstance->XLines->GetFactory(method);
			if (!xlf)
				return CMD_FAILURE;
		}

		const std::string reason = parameters.size() > 2 ? parameters.back() : "Clearing " + chan->name;

		if (!user->server->IsSilentULine())
			ServerInstance->SNO->WriteToSnoMask((IS_LOCAL(user) ? 'a' : 'A'), user->nick + " has cleared \002" + chan->name + "\002 (" + method + "): " + reason);

		user->WriteNotice("Clearing \002" + chan->name + "\002 (" + method + "): " + reason);

		{
			// Route this command manually so it is sent before the QUITs we are about to generate.
			// The idea is that by the time our QUITs reach the next hop, it has already removed all their
			// clients from the channel, meaning victims on other servers won't see the victims on this
			// server quitting.
			std::vector<std::string> eparams;
			eparams.push_back(chan->name);
			eparams.push_back(method);
			eparams.push_back(":");
			eparams.back().append(reason);
			ServerInstance->PI->BroadcastEncap(this->name, eparams, user, user);
		}

		// Attach to the appropriate hook so we're able to hide the QUIT/KICK messages
		Implementation hook = (kick ? I_OnUserKick : I_OnBuildNeighborList);
		ServerInstance->Modules->Attach(hook, creator);

		std::string mask;
		// Now remove all local non-opers from the channel
		Channel::MemberMap& users = chan->userlist;
		for (Channel::MemberMap::iterator i = users.begin(); i != users.end(); )
		{
			User* curr = i->first;
			const Channel::MemberMap::iterator currit = i;
			++i;

			if (!IS_LOCAL(curr) || curr->IsOper())
				continue;

			// If kicking users, remove them and skip the QuitUser()
			if (kick)
			{
				chan->KickUser(ServerInstance->FakeClient, currit, reason);
				continue;
			}

			// If we are banning users then create the XLine and add it
			if (xlf)
			{
				XLine* xline;
				try
				{
					mask = ((method[0] == 'Z') ? curr->GetIPString() : "*@" + curr->host);
					xline = xlf->Generate(ServerInstance->Time(), 60*60, user->nick, reason, mask);
				}
				catch (ModuleException&)
				{
					// Nothing, move on to the next user
					continue;
				}

				if (!ServerInstance->XLines->AddLine(xline, user))
					delete xline;
			}

			ServerInstance->Users->QuitUser(curr, reason);
		}

		ServerInstance->Modules->Detach(hook, creator);
		if (xlf)
			ServerInstance->XLines->ApplyLines();

		return CMD_SUCCESS;
	}
};

class ModuleClearChan : public Module
{
	CommandClearChan cmd;

 public:
	ModuleClearChan()
		: cmd(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		// Only attached while we are working; don't react to events otherwise
		ServerInstance->Modules->DetachAll(this);
	}

	void OnBuildNeighborList(User* source, IncludeChanList& include, std::map<User*, bool>& exception) CXX11_OVERRIDE
	{
		bool found = false;
		for (IncludeChanList::iterator i = include.begin(); i != include.end(); ++i)
		{
			if ((*i)->chan == cmd.activechan)
			{
				// Don't show the QUIT to anyone in the channel by default
				include.erase(i);
				found = true;
				break;
			}
		}

		const Channel::MemberMap& users = cmd.activechan->GetUsers();
		for (Channel::MemberMap::const_iterator i = users.begin(); i != users.end(); ++i)
		{
			LocalUser* curr = IS_LOCAL(i->first);
			if (!curr)
				continue;

			if (curr->IsOper())
			{
				// If another module has removed the channel we're working on from the list of channels
				// to consider for sending the QUIT to then don't add exceptions for opers, because the
				// module before us doesn't want them to see it or added the exceptions already.
				// If there is a value for this oper in excepts already, this won't overwrite it.
				if (found)
					exception.insert(std::make_pair(curr, true));
				continue;
			}
			else if (!include.empty() && curr->chans.size() > 1)
			{
				// This is a victim and potentially has another common channel with the user quitting,
				// add a negative exception overwriting the previous value, if any.
				exception[curr] = false;
			}
		}
	}

	void OnUserKick(User* source, Membership* memb, const std::string& reason, CUList& excepts) CXX11_OVERRIDE
	{
		// Hide the KICK from all non-opers
		User* leaving = memb->user;
		const Channel::MemberMap& users = memb->chan->GetUsers();
		for (Channel::MemberMap::const_iterator i = users.begin(); i != users.end(); ++i)
		{
			User* curr = i->first;
			if ((IS_LOCAL(curr)) && (!curr->IsOper()) && (curr != leaving))
				excepts.insert(curr);
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Adds /CLEARCHAN that allows opers to masskick, masskill or mass-G/ZLine users on a channel", VF_VENDOR|VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleClearChan)
