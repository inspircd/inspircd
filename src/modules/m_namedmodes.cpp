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

class ModuleNamedModes : public Module
{
 public:
	ModuleNamedModes()
	{
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

	void DisplayList(User* user, Channel* channel)
	{
		std::stringstream items;
		for(char letter = 'A'; letter <= 'z'; letter++)
		{
			ModeHandler* mh = ServerInstance->Modes->FindMode(letter, MODETYPE_CHANNEL);
			if (!mh || mh->IsListMode())
				continue;
			if (!channel->IsModeSet(letter))
				continue;
			std::string item = mh->name;
			if (mh->GetNumParams(true))
				item += "=" + channel->GetModeParameter(letter);
			items << item << " ";
		}
		char pfx[MAXBUF];
		snprintf(pfx, MAXBUF, ":%s 961 %s %s", ServerInstance->Config->ServerName.c_str(), user->nick.c_str(), channel->name.c_str());
		user->SendText(std::string(pfx), items);
		user->WriteNumeric(960, "%s %s :End of mode list", user->nick.c_str(), channel->name.c_str());
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
				for(char letter = 'A'; modechar == 0 && letter <= 'z'; letter++)
				{
					mh = ServerInstance->Modes->FindMode(letter, MODETYPE_CHANNEL);
					if (mh && mh->name == name)
					{
						if (mh->GetNumParams(adding))
						{
							if (!value.empty())
							{
								newparms.push_back(value);
								modechar = letter;
								break;
							}
						}
						else
						{
							modechar = letter;
							break;
						}
					}
				}
				if (modechar)
					modelist[i] = modechar;
				else
					modelist.erase(i, 1);
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
