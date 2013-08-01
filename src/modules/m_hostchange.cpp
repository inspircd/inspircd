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

/** Holds information on a host set by m_hostchange
 */
class Host
{
 public:
	enum HostChangeAction
	{
		HCA_SET,
		HCA_SUFFIX,
		HCA_ADDNICK,
		HCA_SPOOF
	};

	std::string mask;
	HostChangeAction action;
	std::string newhost;
	std::string ports;

	std::string connect;
	std::string spoof;

	Host(const std::string &Mask, HostChangeAction Action, const std::string& Newhost, const std::string& Ports, const std::string& Connect, const std::string& Spoof) :
		mask(Mask), action(Action), newhost(Newhost), ports(Ports), connect(Connect), spoof(Spoof) {}
	
	bool Matches(LocalUser *user) const
	{
		if (InspIRCd::MatchCIDR(user->MakeHost(), mask))
			return true;

		if (InspIRCd::MatchCIDR(user->MakeHostIP(), mask))
			return true;

		if (user->MyClass && user->MyClass->name == connect)
			return true;

		return false;
	}
};

typedef std::vector<Host> hostchanges_t;

class ModuleHostChange : public Module
{
	hostchanges_t hostchanges;
	std::string MySuffix;
	std::string MyPrefix;
	std::string MySeparator;

 public:
	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* host = ServerInstance->Config->ConfValue("host");
		MySuffix = host->getString("suffix");
		MyPrefix = host->getString("prefix");
		MySeparator = host->getString("separator", ".");
		hostchanges.clear();

		std::set<std::string> dupecheck;
		ConfigTagList tags = ServerInstance->Config->ConfTags("hostchange");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string mask = tag->getString("mask");
			if (!dupecheck.insert(mask).second)
				throw ModuleException("Duplicate hostchange entry: " + mask);

			Host::HostChangeAction act;
			std::string newhost;
			std::string action = tag->getString("action");
			if (!strcasecmp(action.c_str(), "set"))
			{
				act = Host::HCA_SET;
				newhost = tag->getString("value");
			}
			else if (!strcasecmp(action.c_str(), "suffix"))
				act = Host::HCA_SUFFIX;
			else if (!strcasecmp(action.c_str(), "addnick"))
				act = Host::HCA_ADDNICK;
			else if (!strcasecmp(action.c_str(), "spoof"))
				act = Host::HCA_SPOOF;
			else
				throw ModuleException("Invalid hostchange action: " + action);

			std::string spoof = tag->getString("spoof");

			if (!spoof.find(':') || spoof.find(' ') != std::string::npos || spoof.length() > 64)
				spoof.clear();

			hostchanges.push_back(Host(mask, act, newhost, tag->getString("ports"), tag->getString("connect"), spoof));
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		// returns the version number of the module to be
		// listed in /MODULES
		return Version("Provides masking of user hostnames in a different way to m_cloaking", VF_VENDOR);
	}

	void OnUserConnect(LocalUser* user) CXX11_OVERRIDE
	{
		for (hostchanges_t::iterator i = hostchanges.begin(); i != hostchanges.end(); i++)
		{
			const Host& h = *i;
			
			if (h.Matches(user))
			{
				if (!h.ports.empty())
				{
					irc::portparser portrange(h.ports, false);
					long portno = -1;
					bool foundany = false;

					while ((portno = portrange.GetToken()))
						if (portno == user->GetServerPort())
							foundany = true;

					if (!foundany)
						continue;
				}

				// host of new user matches a hostchange tag's mask
				std::string newhost;
				if (h.action == Host::HCA_SET)
				{
					newhost = h.newhost;
				}
				else if (h.action == Host::HCA_SUFFIX)
				{
					newhost = MySuffix;
				}
				else if (h.action == Host::HCA_ADDNICK)
				{
					// first take their nick and strip out non-dns, leaving just [A-Z0-9\-]
					std::string complete;
					for (std::string::const_iterator j = user->nick.begin(); j != user->nick.end(); ++j)
					{
						if  (((*j >= 'A') && (*j <= 'Z')) ||
						    ((*j >= 'a') && (*j <= 'z')) ||
						    ((*j >= '0') && (*j <= '9')) ||
						    (*j == '-'))
						{
							complete = complete + *j;
						}
					}
					if (complete.empty())
						complete = "i-have-a-lame-nick";

					if (!MyPrefix.empty())
						newhost = MyPrefix + MySeparator + complete;
					else
						newhost = complete + MySeparator + MySuffix;
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
	}

	void OnUserInit(LocalUser* user) CXX11_OVERRIDE
	{
		for (hostchanges_t::iterator i = hostchanges.begin(); i != hostchanges.end(); i++)
		{
			const Host& h = *i;

			if (!h.Matches(user))
				continue;

			if (h.action == Host::HCA_SPOOF && !h.spoof.empty())
			{
				user->host = user->dhost = h.spoof;
				user->InvalidateCache();
				ServerInstance->Users->RemoveCloneCounts(user);
				user->SetClientIP("255.255.255.255", false);
				user->exempt = true;

				user->WriteNotice("Spoofing your host to: " + h.spoof);
				return;
			}
		}
	}
};

MODULE_INIT(ModuleHostChange)
