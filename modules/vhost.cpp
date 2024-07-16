/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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
#include "utility/string.h"

struct CustomVhost final
{
	const std::string name;
	const std::string password;
	const std::string hash;
	const std::string vhost;

	CustomVhost(const std::string& n, const std::string& p, const std::string& h, const std::string& v)
		: name(n)
		, password(p)
		, hash(h)
		, vhost(v)
	{
	}

	bool CheckPass(const std::string& pass) const
	{
		return InspIRCd::CheckPassword(password, hash, pass);
	}
};

typedef std::multimap<std::string, CustomVhost> CustomVhostMap;

class CommandVhost final
	: public Command
{
public:
	CustomVhostMap vhosts;

	CommandVhost(Module* Creator)
		: Command(Creator, "VHOST", 2)
	{
		syntax = { "<username> <password>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		for (const auto& [_, config] : insp::equal_range(vhosts, parameters[0]))
		{
			if (config.CheckPass(parameters[1]))
			{
				user->WriteNotice("Setting your VHost: " + config.vhost);
				user->ChangeDisplayedHost(config.vhost);
				return CmdResult::SUCCESS;
			}
		}

		user->WriteNotice("Invalid username or password.");
		return CmdResult::FAILURE;
	}
};

class ModuleVHost final
	: public Module
{
private:
	CommandVhost cmd;

public:
	ModuleVHost()
		: Module(VF_VENDOR, "Allows the server administrator to define accounts which can grant a custom virtual host.")
		, cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		CustomVhostMap newhosts;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("vhost"))
		{
			std::string mask = tag->getString("host");
			if (mask.empty())
				throw ModuleException(this, "<vhost:host> is empty! at " + tag->source.str());

			std::string username = tag->getString("user");
			if (username.empty())
				throw ModuleException(this, "<vhost:user> is empty! at " + tag->source.str());

			std::string pass = tag->getString("pass");
			if (pass.empty())
				throw ModuleException(this, "<vhost:pass> is empty! at " + tag->source.str());

			const std::string hash = tag->getString("hash", "plaintext", 1);
			if (insp::equalsci(hash, "plaintext"))
			{
				ServerInstance->Logs.Warning(MODNAME, "<vhost> tag for {} at {} contains an plain text password, this is insecure!",
					username, tag->source.str());
			}

			CustomVhost vhost(username, pass, hash, mask);
			newhosts.emplace(username, vhost);
		}

		cmd.vhosts.swap(newhosts);
	}
};

MODULE_INIT(ModuleVHost)
