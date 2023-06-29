/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2017-2018, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014, 2016 Attila Molnar <attilamolnar@hush.com>
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

class CommandClearChan final
	: public Command
{
public:
	Channel* activechan;

	CommandClearChan(Module* Creator)
		: Command(Creator, "CLEARCHAN", 1, 3)
	{
		syntax = { "<channel> [KILL|KICK|G|Z] [:<reason>]" };
		access_needed = CmdAccess::OPERATOR;

		// Stop the linking mod from forwarding ENCAP'd CLEARCHAN commands, see below why
		force_manual_route = true;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		Channel* chan = activechan = ServerInstance->Channels.Find(parameters[0]);
		if (!chan)
		{
			user->WriteNotice("The channel " + parameters[0] + " does not exist.");
			return CmdResult::FAILURE;
		}

		// See what method the oper wants to use, default to KILL
		std::string method("KILL");
		if (parameters.size() > 1)
		{
			method = parameters[1];
			std::transform(method.begin(), method.end(), method.begin(), ::toupper);
		}

		XLineFactory* xlf = nullptr;
		bool kick = (method == "KICK");
		if ((!kick) && (method != "KILL"))
		{
			if ((method != "Z") && (method != "G"))
			{
				user->WriteNotice("Invalid method for clearing " + chan->name);
				return CmdResult::FAILURE;
			}

			xlf = ServerInstance->XLines->GetFactory(method);
			if (!xlf)
				return CmdResult::FAILURE;
		}

		const std::string reason = parameters.size() > 2 ? parameters.back() : "Clearing " + chan->name;

		if (!user->server->IsSilentService())
			ServerInstance->SNO.WriteToSnoMask((IS_LOCAL(user) ? 'a' : 'A'), user->nick + " has cleared \002" + chan->name + "\002 (" + method + "): " + reason);

		user->WriteNotice("Clearing \002" + chan->name + "\002 (" + method + "): " + reason);

		{
			// Route this command manually so it is sent before the QUITs we are about to generate.
			// The idea is that by the time our QUITs reach the next hop, it has already removed all their
			// clients from the channel, meaning victims on other servers won't see the victims on this
			// server quitting.
			CommandBase::Params eparams;
			eparams.push_back(chan->name);
			eparams.push_back(method);
			eparams.push_back(":");
			eparams.back().append(reason);
			ServerInstance->PI->BroadcastEncap(this->name, eparams, user, user);
		}

		// Attach to the appropriate hook so we're able to hide the QUIT/KICK messages
		Implementation hook = (kick ? I_OnUserKick : I_OnBuildNeighborList);
		ServerInstance->Modules.Attach(hook, creator);

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
					mask = (method[0] == 'Z') ? curr->GetAddress() : (curr->GetBanUser(true) + "@" + curr->GetRealHost());
					xline = xlf->Generate(ServerInstance->Time(), 60*60, user->nick, reason, mask);
				}
				catch (const ModuleException&)
				{
					// Nothing, move on to the next user
					continue;
				}

				if (!ServerInstance->XLines->AddLine(xline, user))
					delete xline;
			}

			ServerInstance->Users.QuitUser(curr, reason);
		}

		ServerInstance->Modules.Detach(hook, creator);
		if (xlf)
			ServerInstance->XLines->ApplyLines();

		return CmdResult::SUCCESS;
	}
};

class ModuleClearChan final
	: public Module
{
private:
	CommandClearChan cmd;

public:
	ModuleClearChan()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /CLEARCHAN command which allows server operators to mass-punish the members of a channel.")
		, cmd(this)
	{
	}

	void init() override
	{
		// Only attached while we are working; don't react to events otherwise
		Implementation events[] = { I_OnBuildNeighborList, I_OnUserKick };
		ServerInstance->Modules.Detach(events, this, sizeof(events)/sizeof(Implementation));
	}

	void OnBuildNeighborList(User* source, User::NeighborList& include, User::NeighborExceptions& exception) override
	{
		bool found = false;
		for (User::NeighborList::iterator i = include.begin(); i != include.end(); ++i)
		{
			if ((*i)->chan == cmd.activechan)
			{
				// Don't show the QUIT to anyone in the channel by default
				include.erase(i);
				found = true;
				break;
			}
		}

		for (const auto& [member, _] : cmd.activechan->GetUsers())
		{
			LocalUser* curr = IS_LOCAL(member);
			if (!curr)
				continue;

			if (curr->IsOper())
			{
				// If another module has removed the channel we're working on from the list of channels
				// to consider for sending the QUIT to then don't add exceptions for opers, because the
				// module before us doesn't want them to see it or added the exceptions already.
				// If there is a value for this oper in excepts already, this won't overwrite it.
				if (found)
					exception.emplace(curr, true);
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

	void OnUserKick(User* source, Membership* memb, const std::string& reason, CUList& excepts) override
	{
		// Hide the KICK from all non-opers
		User* leaving = memb->user;
		for (const auto& [curr, _] : memb->chan->GetUsers())
		{
			if ((IS_LOCAL(curr)) && (!curr->IsOper()) && (curr != leaving))
				excepts.insert(curr);
		}
	}
};

MODULE_INIT(ModuleClearChan)
