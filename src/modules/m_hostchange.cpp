/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2005-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
	insp::flat_set<int> ports;
	std::string prefix;
	std::string suffix;

 public:
	HostRule(const std::string& Host, const std::string& Mask, const insp::flat_set<int>& Ports)
		: action(HCA_SET)
		, host(Host)
		, mask(Mask)
		, ports(Ports)
	{
	}

	HostRule(HostChangeAction Action, const std::string& Mask, const insp::flat_set<int>& Ports, const std::string& Prefix, const std::string& Suffix)
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
		if (!ports.empty() && !ports.count(user->GetServerPort()))
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
	std::bitset<UCHAR_MAX> hostmap;
	HostRules hostrules;

	std::string CleanName(const std::string& name)
	{
		std::string buffer;
		buffer.reserve(name.length());
		for (std::string::const_iterator iter = name.begin(); iter != name.end(); ++iter)
		{
			if (hostmap.test(static_cast<unsigned char>(*iter)))
			{
				buffer.push_back(*iter);
			}
		}
		return buffer;
	}

 public:
	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		HostRules rules;

		ConfigTagList tags = ServerInstance->Config->ConfTags("hostchange");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;

			// Ensure that we have the <hostchange:mask> parameter.
			const std::string mask = tag->getString("mask");
			if (mask.empty())
				throw ModuleException("<hostchange:mask> is a mandatory field, at " + tag->getTagLocation());

			insp::flat_set<int> ports;
			const std::string portlist = tag->getString("ports");
			if (!ports.empty())
			{
				irc::portparser portrange(portlist, false);
				while (int port = portrange.GetToken())
					ports.insert(port);
			}

			// Determine what type of host rule this is.
			const std::string action = tag->getString("action");
			if (stdalgo::string::equalsci(action, "addaccount"))
			{
				// The hostname is in the format [prefix]<account>[suffix].
				rules.push_back(HostRule(HostRule::HCA_ADDACCOUNT, mask, ports, tag->getString("prefix"), tag->getString("suffix")));
			}
			else if (stdalgo::string::equalsci(action, "addnick"))
			{
				// The hostname is in the format [prefix]<nick>[suffix].
				rules.push_back(HostRule(HostRule::HCA_ADDNICK, mask, ports, tag->getString("prefix"), tag->getString("suffix")));
			}
			else if (stdalgo::string::equalsci(action, "set"))
			{
				// Ensure that we have the <hostchange:value> parameter.
				const std::string value = tag->getString("value");
				if (value.empty())
					throw ModuleException("<hostchange:value> is a mandatory field when using the 'set' action, at " + tag->getTagLocation());

				// The hostname is in the format <value>.
				rules.push_back(HostRule(mask, value, ports));
				continue;
			}
			else
			{
				throw ModuleException(action + " is an invalid <hostchange:action> type, at " + tag->getTagLocation()); 
			}
		}

		const std::string hmap = ServerInstance->Config->ConfValue("hostname")->getString("charmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789");
		hostmap.reset();
		for (std::string::const_iterator iter = hmap.begin(); iter != hmap.end(); ++iter)
			hostmap.set(static_cast<unsigned char>(*iter));
		hostrules.swap(rules);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides rule-based masking of user hostnames", VF_VENDOR);
	}

	void OnUserConnect(LocalUser* user) CXX11_OVERRIDE
	{
		for (HostRules::const_iterator iter = hostrules.begin(); iter != hostrules.end(); ++iter)
		{
			const HostRule& rule = *iter;
			if (!rule.Matches(user))
				continue;

			std::string newhost;
			if (rule.GetAction() == HostRule::HCA_ADDACCOUNT)
			{
				// Retrieve the account name.
				const AccountExtItem* accountext = GetAccountExtItem();
				const std::string* accountptr = accountext ? accountext->get(user) : NULL;
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
