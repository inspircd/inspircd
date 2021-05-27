/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007, 2010 Craig Edwards <brain@inspircd.org>
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
#include "modules/account.h"

// Holds information about a <hostchange> rule.
class HostRule
{
 public:
	enum HostChangeAction
	{
		// Add the user's account name to their hostname.
		HCA_ADDACCOUNT,

		// Add the user's nickname to their hostname.
		HCA_ADDNICK,

		// Set the user's hostname to the specific value.
		HCA_SET
	};

 private:
	HostChangeAction action;
	std::string host;
	std::string mask;
	insp::flat_set<long> ports;
	std::string prefix;
	std::string suffix;

 public:
	HostRule(const std::string& Mask, const std::string& Host, const insp::flat_set<long>& Ports)
		: action(HCA_SET)
		, host(Host)
		, mask(Mask)
		, ports(Ports)
	{
	}

	HostRule(HostChangeAction Action, const std::string& Mask, const insp::flat_set<long>& Ports, const std::string& Prefix, const std::string& Suffix)
		: action(Action)
		, mask(Mask)
		, ports(Ports)
		, prefix(Prefix)
		, suffix(Suffix)
	{
	}

	HostChangeAction GetAction() const
	{
		return action;
	}

	const std::string& GetHost() const
	{
		return host;
	}

	bool Matches(LocalUser* user) const
	{
		if (!ports.empty() && !ports.count(user->server_sa.port()))
			return false;

		if (InspIRCd::MatchCIDR(user->MakeHost(), mask))
			return true;

		return InspIRCd::MatchCIDR(user->MakeHostIP(), mask);
	}

	void Wrap(const std::string& value, std::string& out) const
	{
		if (!prefix.empty())
			out.append(prefix);

		out.append(value);

		if (!suffix.empty())
			out.append(suffix);
	}
};

typedef std::vector<HostRule> HostRules;

class ModuleHostChange : public Module
{
private:
	std::bitset<UCHAR_MAX + 1> hostmap;
	HostRules hostrules;

	std::string CleanName(const std::string& name)
	{
		std::string buffer;
		buffer.reserve(name.length());
		for (const auto& chr : name)
		{
			if (hostmap.test(static_cast<unsigned char>(chr)))
				buffer.push_back(chr);
		}
		return buffer;
	}

 public:
	ModuleHostChange()
		: Module(VF_VENDOR, "Allows the server administrator to define custom rules for applying hostnames to users.")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		HostRules rules;

		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("hostchange"))
		{
			// Ensure that we have the <hostchange:mask> parameter.
			const std::string mask = tag->getString("mask");
			if (mask.empty())
				throw ModuleException("<hostchange:mask> is a mandatory field, at " + tag->source.str());

			insp::flat_set<long> ports;
			const std::string portlist = tag->getString("ports");
			if (!portlist.empty())
			{
				irc::portparser portrange(portlist, false);
				while (long port = portrange.GetToken())
					ports.insert(port);
			}

			// Determine what type of host rule this is.
			const std::string action = tag->getString("action");
			if (stdalgo::string::equalsci(action, "addaccount"))
			{
				// The hostname is in the format [prefix]<account>[suffix].
				rules.emplace_back(HostRule::HCA_ADDACCOUNT, mask, ports, tag->getString("prefix"), tag->getString("suffix"));
			}
			else if (stdalgo::string::equalsci(action, "addnick"))
			{
				// The hostname is in the format [prefix]<nick>[suffix].
				rules.emplace_back(HostRule::HCA_ADDNICK, mask, ports, tag->getString("prefix"), tag->getString("suffix"));
			}
			else if (stdalgo::string::equalsci(action, "set"))
			{
				// Ensure that we have the <hostchange:value> parameter.
				const std::string value = tag->getString("value");
				if (value.empty())
					throw ModuleException("<hostchange:value> is a mandatory field when using the 'set' action, at " + tag->source.str());

				// The hostname is in the format <value>.
				rules.emplace_back(mask, value, ports);
				continue;
			}
			else
			{
				throw ModuleException(action + " is an invalid <hostchange:action> type, at " + tag->source.str());
			}
		}

		auto tag = ServerInstance->Config->ConfValue("hostname");
		const std::string hmap = tag->getString("charmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789", 1);

		hostmap.reset();
		for (const auto& chr : hmap)
			hostmap.set(static_cast<unsigned char>(chr));
		hostrules.swap(rules);
	}

	void OnUserConnect(LocalUser* user) override
	{
		for (const auto& rule : hostrules)
		{
			if (!rule.Matches(user))
				continue;

			std::string newhost;
			if (rule.GetAction() == HostRule::HCA_ADDACCOUNT)
			{
				// Retrieve the account name.
				const AccountExtItem* accountext = GetAccountExtItem();
				const std::string* accountptr = accountext ? accountext->Get(user) : NULL;
				if (!accountptr)
					continue;

				// Remove invalid hostname characters.
				std::string accountname = CleanName(*accountptr);
				if (accountname.empty())
					continue;

				// Create the hostname.
				rule.Wrap(accountname, newhost);
			}
			else if (rule.GetAction() == HostRule::HCA_ADDNICK)
			{
				// Remove invalid hostname characters.
				const std::string nickname = CleanName(user->nick);
				if (nickname.empty())
					continue;

				// Create the hostname.
				rule.Wrap(nickname, newhost);
			}
			else if (rule.GetAction() == HostRule::HCA_SET)
			{
				newhost.assign(rule.GetHost());
			}

			if (!newhost.empty())
			{
				user->WriteNotice("Setting your virtual host: " + newhost);
				if (!user->ChangeDisplayedHost(newhost))
					user->WriteNotice("Could not set your virtual host: " + newhost);
				return;
			}
		}
	}
};

MODULE_INIT(ModuleHostChange)
