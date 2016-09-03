/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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

class ModulePassForward : public Module
{
	std::string nickrequired, forwardmsg, forwardcmd;

 public:
	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Sends server password to NickServ", VF_VENDOR);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("passforward");
		nickrequired = tag->getString("nick", "NickServ");
		forwardmsg = tag->getString("forwardmsg", "NOTICE $nick :*** Forwarding PASS to $nickrequired");
		forwardcmd = tag->getString("cmd", "PRIVMSG $nickrequired :IDENTIFY $pass");
	}

	void FormatStr(std::string& result, const std::string& format, const LocalUser* user)
	{
		for (unsigned int i = 0; i < format.length(); i++)
		{
			char c = format[i];
			if (c == '$')
			{
				if (!format.compare(i, 13, "$nickrequired", 13))
				{
					result.append(nickrequired);
					i += 12;
				}
				else if (!format.compare(i, 5, "$nick", 5))
				{
					result.append(user->nick);
					i += 4;
				}
				else if (!format.compare(i, 5, "$user", 5))
				{
					result.append(user->ident);
					i += 4;
				}
				else if (!format.compare(i, 5, "$pass", 5))
				{
					result.append(user->password);
					i += 4;
				}
				else
					result.push_back(c);
			}
			else
				result.push_back(c);
		}
	}

	void OnPostConnect(User* ruser) CXX11_OVERRIDE
	{
		LocalUser* user = IS_LOCAL(ruser);
		if (!user || user->password.empty())
			return;

		// If the connect class requires a password, don't forward it
		if (!user->MyClass->config->getString("password").empty())
			return;

		if (!nickrequired.empty())
		{
			/* Check if nick exists and its server is ulined */
			User* u = ServerInstance->FindNick(nickrequired);
			if (!u || !u->server->IsULine())
				return;
		}

		std::string tmp;
		FormatStr(tmp,forwardmsg, user);
		user->WriteServ(tmp);

		tmp.clear();
		FormatStr(tmp,forwardcmd, user);
		ServerInstance->Parser.ProcessBuffer(tmp,user);
	}
};

MODULE_INIT(ModulePassForward)
