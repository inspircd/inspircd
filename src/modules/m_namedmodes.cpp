/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

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
		items << " " << item;
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

			ModeHandler* mh = ServerInstance->Modes->FindMode(prop);
			if (mh && mh->GetModeType() == MODETYPE_CHANNEL)
			{
				modes[1].append((plus ? "+" : "-") + std::string(1, letter));
				if (mh->GetNumParams(plus))
				{
					if (i == parameters.size())
						return CMD_FAILURE;
					modes.push_back(parameters[i++]);
				}
			}
		}
		ServerInstance->SendGlobalMode(modes, src);
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

		Implementation eventlist[] = { I_OnPreMode, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	Version GetVersion()
	{
		return Version("Provides the ability to manipulate modes via long names.",VF_VENDOR);
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnPreMode, PRIORITY_FIRST);
	}

	void On005Numeric(std::string& line)
	{
		std::string::size_type pos = line.find(" CHANMODES=");
		if (pos != std::string::npos)
		{
			pos += 11;
			while (line[pos] > 'A' && line[pos] < 'Z')
				pos++;
			line.insert(pos, 1, 'Z');
		}
	}

	ModResult OnPreMode(User* source, User* dest, Channel* channel, const std::vector<std::string>& parameters)
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
				modechar = 0;
				std::string name, value;
				if (param_at < parameters.size())
					name = parameters[param_at++];
				std::string::size_type eq = name.find('=');
				if (eq != std::string::npos)
				{
					value = name.substr(eq + 1);
					name = name.substr(0, eq);
				}
				mh = ServerInstance->Modes->FindMode(name);
				if (mh && mh->GetModeType() == MODETYPE_CHANNEL)
				{
					if (mh->GetNumParams(adding))
					{
						if (!value.empty())
						{
							newparms.push_back(value);
							modechar = mh->GetModeChar();
							break;
						}
					}
					else
					{
						modechar = mh->GetModeChar();
						break;
					}
				}
				if (modechar)
					modelist[i] = modechar;
				else
					modelist.erase(i--, 1);
			}
			else if (mh && mh->GetNumParams(adding) && param_at < parameters.size())
			{
				newparms.push_back(parameters[param_at++]);
			}
		}
		newparms[1] = modelist;
		ServerInstance->Modes->Process(newparms, source, false);
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleNamedModes)
