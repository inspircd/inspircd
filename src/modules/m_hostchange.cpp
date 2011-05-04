/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides masking of user hostnames in a different way to m_cloaking */

/** Holds information on a host set by m_hostchange
 */
class Host
{
 public:
	std::string action;
	std::string newhost;
	std::string ports;
};

typedef std::map<std::string,Host*> hostchanges_t;

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
		Implementation eventlist[] = { I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ~ModuleHostChange()
	{
		for (hostchanges_t::iterator i = hostchanges.begin(); i != hostchanges.end(); i++)
		{
			delete i->second;
		}
		hostchanges.clear();
	}

	void Prioritize()
	{
		Module* cloak = ServerInstance->Modules->Find("m_cloaking.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserConnect, PRIORITY_AFTER, cloak);
	}


	void ReadConfig(ConfigReadStatus&)
	{
		ConfigTag* tag = ServerInstance->Config->GetTag("host");
		MySuffix = tag->getString("suffix");
		MyPrefix = tag->getString("prefix");
		MySeparator = tag->getString("separator",".");
		for (hostchanges_t::iterator i = hostchanges.begin(); i != hostchanges.end(); i++)
		{
			delete i->second;
		}
		hostchanges.clear();
		ConfigTagList tags = ServerInstance->Config->GetTags("hostchange");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			std::string mask = i->second->getString("mask");
			std::string ports = i->second->getString("ports");
			std::string action = i->second->getString("action");
			std::string newhost = i->second->getString("value");
			Host* x = new Host;
			x->action = action;
			x->ports = ports;
			x->newhost = newhost;
			hostchanges[mask] = x;
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
				Host* h = i->second;

				if (!h->ports.empty())
				{
					irc::portparser portrange(h->ports, false);
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
				if (h->action == "set")
				{
					newhost = h->newhost;
				}
				else if (h->action == "suffix")
				{
					newhost = MySuffix;
				}
				else if (h->action == "addnick")
				{
					// first take their nick and strip out non-dns, leaving just [A-Z0-9\-]
					std::string complete;
					std::string old = user->nick;
					for (unsigned int j = 0; j < old.length(); j++)
					{
						if  (((old[j] >= 'A') && (old[j] <= 'Z')) ||
						    ((old[j] >= 'a') && (old[j] <= 'z')) ||
						    ((old[j] >= '0') && (old[j] <= '9')) ||
						    (old[j] == '-'))
						{
							complete = complete + old[j];
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
					user->WriteServ("NOTICE %s :Setting your virtual host: %s", user->nick.c_str(), newhost.c_str());
					if (!user->ChangeDisplayedHost(newhost.c_str()))
						user->WriteServ("NOTICE %s :Could not set your virtual host: %s", user->nick.c_str(), newhost.c_str());
					return;
				}
			}
		}
	}
};

MODULE_INIT(ModuleHostChange)
