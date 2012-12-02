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

/* $ModDesc: Provides masking of user hostnames in a different way to m_cloaking */

/** Holds information on a host set by m_hostchange
 */
class Host
{
 public:
	enum HostChangeAction
	{
		HCA_SET,
		HCA_SUFFIX,
		HCA_ADDNICK
	};

	HostChangeAction action;
	std::string newhost;
	std::string ports;

	Host(HostChangeAction Action, const std::string& Newhost, const std::string& Ports) :
		action(Action), newhost(Newhost), ports(Ports) {}
};

typedef std::vector<std::pair<std::string, Host> > hostchanges_t;

class ModuleHostChange : public Module
{
 private:
	hostchanges_t hostchanges;
	std::string MySuffix;
	std::string MyPrefix;
	std::string MySeparator;

 public:
	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual void OnRehash(User* user)
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
			else
				throw ModuleException("Invalid hostchange action: " + action);

			hostchanges.push_back(std::make_pair(mask, Host(act, newhost, tag->getString("ports"))));
		}
	}

	virtual Version GetVersion()
	{
		// returns the version number of the module to be
		// listed in /MODULES
		return Version("Provides masking of user hostnames in a different way to m_cloaking", VF_VENDOR);
	}

	virtual void OnUserConnect(LocalUser* user)
	{
		for (hostchanges_t::iterator i = hostchanges.begin(); i != hostchanges.end(); i++)
		{
			if (((InspIRCd::MatchCIDR(user->MakeHost(), i->first)) || (InspIRCd::MatchCIDR(user->MakeHostIP(), i->first))))
			{
				const Host& h = i->second;

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
					user->WriteServ("NOTICE "+user->nick+" :Setting your virtual host: " + newhost);
					if (!user->ChangeDisplayedHost(newhost.c_str()))
						user->WriteServ("NOTICE "+user->nick+" :Could not set your virtual host: " + newhost);
					return;
				}
			}
		}
	}
};

MODULE_INIT(ModuleHostChange)
