/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "extension.h"
#include "modules/whois.h"

struct CustomTitle final
{
	const std::string name;
	const std::string password;
	const std::string hash;
	const std::string host;
	const std::string title;
	const std::string vhost;

	CustomTitle(const std::string& n, const std::string& p, const std::string& h, const std::string& hst, const std::string& t, const std::string& v)
		: name(n)
		, password(p)
		, hash(h)
		, host(hst)
		, title(t)
		, vhost(v)
	{
	}

	bool MatchUser(User* user) const
	{
		return InspIRCd::MatchMask(host, user->GetRealUserHost(), user->GetUserAddress());
	}

	bool CheckPass(const std::string& pass) const
	{
		return InspIRCd::CheckPassword(password, hash, pass);
	}
};

typedef std::multimap<std::string, CustomTitle> CustomVhostMap;

class CommandTitle final
	: public Command
{
public:
	StringExtItem ctitle;
	CustomVhostMap configs;

	CommandTitle(Module* Creator)
		: Command(Creator, "TITLE", 2)
		, ctitle(Creator, "ctitle", ExtensionType::USER, true)
	{
		syntax = { "<username> <password>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		for (const auto& [_, config] : insp::equal_range(configs, parameters[0]))
		{
			if (config.MatchUser(user) && config.CheckPass(parameters[1]))
			{
				ctitle.Set(user, config.title);

				if (!config.vhost.empty())
					user->ChangeDisplayedHost(config.vhost);

				user->WriteNotice("Custom title set to '" + config.title + "'");

				return CmdResult::SUCCESS;
			}
		}

		user->WriteNotice("Invalid title credentials");
		return CmdResult::SUCCESS;
	}

};

class ModuleCustomTitle final
	: public Module
	, public Whois::LineEventListener
{
private:
	CommandTitle cmd;

public:
	ModuleCustomTitle()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Allows the server administrator to define accounts which can grant a custom title in /WHOIS and an optional virtual host.")
		, Whois::LineEventListener(this)
		, cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		CustomVhostMap newtitles;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("title"))
		{
			std::string name = tag->getString("name", "", 1);
			if (name.empty())
				throw ModuleException(this, "<title:name> is empty at " + tag->source.str());

			std::string pass = tag->getString("password");
			if (pass.empty())
				throw ModuleException(this, "<title:password> is empty at " + tag->source.str());

			const std::string hash = tag->getString("hash", "plaintext", 1);
			if (insp::equalsci(hash, "plaintext"))
			{
				ServerInstance->Logs.Normal(MODNAME, "<title> tag for {} at {} contains an plain text password, this is insecure!",
					name, tag->source.str());
			}

			std::string host = tag->getString("host", "*@*", 1);
			std::string title = tag->getString("title");
			std::string vhost = tag->getString("vhost");
			CustomTitle config(name, pass, hash, host, title, vhost);
			newtitles.emplace(name, config);
		}
		cmd.configs.swap(newtitles);
	}

	// :kenny.chatspike.net 320 Brain Azhrarn :is getting paid to play games.
	ModResult OnWhoisLine(Whois::Context& whois, Numeric::Numeric& numeric) override
	{
		/* We use this and not OnWhois because this triggers for remote, too */
		if (numeric.GetNumeric() == RPL_WHOISSERVER)
		{
			/* Insert our numeric before 312 */
			const std::string* ctitle = cmd.ctitle.Get(whois.GetTarget());
			if (ctitle && !ctitle->empty())
				whois.SendLine(RPL_WHOISSPECIAL, *ctitle);
		}
		/* Don't block anything */
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleCustomTitle)
