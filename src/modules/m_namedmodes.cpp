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


#include "inspircd.h"

static void DisplayList(User* user, Channel* channel)
{
	std::stringstream items;
	for(char letter = 'A'; letter <= 'z'; letter++)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(letter, MODETYPE_CHANNEL);
		if (!mh || mh->IsListMode())
			continue;
		if (!channel->IsModeSet(mh))
			continue;
		items << " +" << mh->name;
		if (mh->GetNumParams(true))
			items << " " << channel->GetModeParameter(mh);
	}
	const std::string line = ":" + ServerInstance->Config->ServerName + " 961 " + user->nick + " " + channel->name;
	user->SendText(line, items);
	user->WriteNumeric(960, "%s :End of mode list", channel->name.c_str());
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
		if (parameters.size() == 1)
		{
			Channel* chan = ServerInstance->FindChan(parameters[0]);
			if (chan)
				DisplayList(src, chan);
			return CMD_SUCCESS;
		}
		unsigned int i = 1;
		std::vector<std::string> modes;
		modes.push_back(parameters[0]);
		modes.push_back("");
		while (i < parameters.size())
		{
			std::string prop = parameters[i++];
			bool plus = prop[0] != '-';
			if (prop[0] == '+' || prop[0] == '-')
				prop.erase(prop.begin());

			ModeHandler* mh = ServerInstance->Modes->FindMode(prop, MODETYPE_CHANNEL);
			if (mh)
			{
				modes[1].push_back(plus ? '+' : '-');
				modes[1].push_back(mh->GetModeChar());
				if (mh->GetNumParams(plus))
				{
					if (i != parameters.size())
						modes.push_back(parameters[i++]);
				}
			}
		}
		ServerInstance->Modes->Process(modes, src);
		return CMD_SUCCESS;
	}
};

class DummyZ : public ModeHandler
{
 public:
	DummyZ(Module* parent) : ModeHandler(parent, "namebase", 'Z', PARAM_ALWAYS, MODETYPE_CHANNEL)
	{
		list = true;
	}
};

class ModuleNamedModes : public Module
{
	CommandProp cmd;
	DummyZ dummyZ;
 public:
	ModuleNamedModes() : cmd(this), dummyZ(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the ability to manipulate modes via long names.",VF_VENDOR);
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnPreMode, PRIORITY_FIRST);
	}

	ModResult OnPreMode(User* source, User* dest, Channel* channel, const std::vector<std::string>& parameters) CXX11_OVERRIDE
	{
		if (!channel)
			return MOD_RES_PASSTHRU;
		if (parameters[1].find('Z') == std::string::npos)
			return MOD_RES_PASSTHRU;
		if (parameters.size() <= 2)
		{
			DisplayList(source, channel);
			return MOD_RES_DENY;
		}

		std::vector<std::string> newparms;
		newparms.push_back(parameters[0]);
		newparms.push_back(parameters[1]);

		std::string modelist = newparms[1];
		bool adding = true;
		unsigned int param_at = 2;
		for(unsigned int i = 0; i < modelist.length(); i++)
		{
			unsigned char modechar = modelist[i];
			if (modechar == '+' || modechar == '-')
			{
				adding = (modechar == '+');
				continue;
			}
			ModeHandler *mh = ServerInstance->Modes->FindMode(modechar, MODETYPE_CHANNEL);
			if (modechar == 'Z')
			{
				std::string name, value;
				if (param_at < parameters.size())
					name = parameters[param_at++];
				std::string::size_type eq = name.find('=');
				if (eq != std::string::npos)
				{
					value = name.substr(eq + 1);
					name = name.substr(0, eq);
				}

				mh = ServerInstance->Modes->FindMode(name, MODETYPE_CHANNEL);
				if (!mh)
				{
					// Mode handler not found
					modelist.erase(i--, 1);
					continue;
				}

				if (mh->GetNumParams(adding))
				{
					if (value.empty())
					{
						// Mode needs a parameter but there wasn't one
						modelist.erase(i--, 1);
						continue;
					}

					newparms.push_back(value);
				}

				modelist[i] = mh->GetModeChar();
			}
			else if (mh && mh->GetNumParams(adding) && param_at < parameters.size())
			{
				newparms.push_back(parameters[param_at++]);
			}
		}
		newparms[1] = modelist;
		ServerInstance->Modes->Process(newparms, source);
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleNamedModes)
