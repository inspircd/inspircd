/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

/** Handle /TITLE
 */
class CommandTitle : public Command
{
 public:
	StringExtItem ctitle;
	CommandTitle(Module* Creator) : Command(Creator,"TITLE", 2),
		ctitle("ctitle", ExtensionItem::EXT_USER, Creator)
	{
		syntax = "<user> <password>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User* user)
	{
		const std::string userHost = user->ident + "@" + user->host;
		const std::string userIP = user->ident + "@" + user->GetIPString();

		ConfigTagList tags = ServerInstance->Config->ConfTags("title");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			std::string Name = i->second->getString("name");
			std::string pass = i->second->getString("password");
			std::string hash = i->second->getString("hash");
			std::string host = i->second->getString("host", "*@*");
			std::string title = i->second->getString("title");
			std::string vhost = i->second->getString("vhost");

			if (Name == parameters[0] && ServerInstance->PassCompare(user, pass, parameters[1], hash) &&
				InspIRCd::MatchMask(host, userHost, userIP) && !title.empty())
			{
				ctitle.set(user, title);

				ServerInstance->PI->SendMetaData(user, "ctitle", title);

				if (!vhost.empty())
					user->ChangeDisplayedHost(vhost);

				user->WriteNotice("Custom title set to '" + title + "'");

				return CMD_SUCCESS;
			}
		}

		user->WriteNotice("Invalid title credentials");
		return CMD_SUCCESS;
	}

};

class ModuleCustomTitle : public Module, public Whois::LineEventListener
{
	CommandTitle cmd;

 public:
	ModuleCustomTitle()
		: Whois::LineEventListener(this)
		, cmd(this)
	{
	}

	// :kenny.chatspike.net 320 Brain Azhrarn :is getting paid to play games.
	ModResult OnWhoisLine(Whois::Context& whois, unsigned int& numeric, std::string& text) CXX11_OVERRIDE
	{
		/* We use this and not OnWhois because this triggers for remote, too */
		if (numeric == 312)
		{
			/* Insert our numeric before 312 */
			const std::string* ctitle = cmd.ctitle.get(whois.GetTarget());
			if (ctitle)
			{
				whois.SendLine(320, ":%s", ctitle->c_str());
			}
		}
		/* Don't block anything */
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Custom Title for users", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleCustomTitle)
