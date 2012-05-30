/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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


/* $ModDesc: Provides the ability to manipulate modes via long names. */

#include "inspircd.h"

static void DisplayList(User* user, Channel* channel)
{
	std::stringstream items;
	for(ModeIDIter id; id; id++)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(id);
		if (!mh || mh->IsListMode() || mh->GetModeType() != MODETYPE_CHANNEL)
			continue;
		if (!channel->IsModeSet(mh))
			continue;
		items << " +" << mh->name;
		if (mh->GetNumParams(true))
			items << " " << channel->GetModeParameter(mh);
	}
	char pfx[MAXBUF];
	snprintf(pfx, MAXBUF, ":%s 961 %s %s", ServerInstance->Config->ServerName.c_str(), user->nick.c_str(), channel->name.c_str());
	user->SendText(std::string(pfx), items);
	user->WriteNumeric(960, "%s %s :End of mode list", user->nick.c_str(), channel->name.c_str());
}

class CommandProp : public Command
{
 public:
	CommandProp(Module* parent) : Command(parent, "PROP", 1)
	{
		syntax = "<user|channel> {[+-]<mode> [<value>]}*";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *src)
	{
		Channel* chan = ServerInstance->FindChan(parameters[0]);
		User* user = ServerInstance->FindNick(parameters[0]);
		if (!chan && !user)
		{
			if (parameters[0] == "*")
			{
				std::string pfx = ":" + ServerInstance->Config->ServerName + " 020 " + src->nick;
				std::set<std::string> modeset;
				for(ModeIDIter id; id; id++)
				{
					ModeHandler* mh = ServerInstance->Modes->FindMode(id);
					if (mh)
						modeset.insert(mh->name);
				}
				std::stringstream dump;
				for(std::set<std::string>::iterator i = modeset.begin(); i != modeset.end(); i++)
					dump << " " << *i;
				src->SendText(pfx, dump);
				return CMD_SUCCESS;
			}
			src->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",src->nick.c_str(),parameters[0].c_str());
			return CMD_FAILURE;
		}
		if (parameters.size() == 1)
		{
			if (chan)
				DisplayList(src, chan);
			return CMD_SUCCESS;
		}
		unsigned int i = 1;
		irc::modestacker modes;
		while (i < parameters.size())
		{
			std::string prop = parameters[i++];
			bool plus = prop[0] != '-';
			if (prop[0] == '+' || prop[0] == '-')
				prop.erase(prop.begin());

			ModeHandler* mh = ServerInstance->Modes->FindMode(prop);
			if (mh && mh->GetModeType() == MODETYPE_CHANNEL)
			{
				irc::modechange mc(mh->id);
				mc.adding = plus;
				if (mh->GetNumParams(plus))
				{
					if (i == 2 && parameters.size() == 2 && chan && mh->IsListMode())
					{
						// special case: display list mode
						mh->DisplayList(src, chan);
						return CMD_SUCCESS;
					}
					if (i == parameters.size())
					{
						src->WriteNumeric(ERR_NEEDMOREPARAMS, "%s PROP :Not enough parameters.", src->nick.c_str());
						return CMD_FAILURE;
					}
					mc.value = parameters[i++];
				}
				modes.push(mc);
			}
		}
		ServerInstance->SendMode(src, chan ? (Extensible*)chan : (Extensible*)user, modes, true);
		return CMD_SUCCESS;
	}
};

class ModuleNamedModes : public Module
{
	CommandProp cmd;
 public:
	ModuleNamedModes() : cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	Version GetVersion()
	{
		return Version("Provides the ability to manipulate modes via long names.",VF_VENDOR);
	}
};

MODULE_INIT(ModuleNamedModes)
