/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides masking of user hostnames in a different way to m_cloaking */

/** Holds information on a host set by m_hostchange
 */
class Host : public classbase
{
 public:
	std::string action;
	std::string newhost;
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
	ModuleHostChange(InspIRCd* Me)
		: Module(Me)
	{
		OnRehash(NULL,"");
	}
	
	virtual ~ModuleHostChange()
	{
		for (hostchanges_t::iterator i = hostchanges.begin(); i != hostchanges.end(); i++)
		{
			DELETE(i->second);
		}
		hostchanges.clear();
	}

	Priority Prioritize()
	{
		return (Priority)ServerInstance->PriorityAfter("m_cloaking.so");
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnUserConnect] = 1;
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		ConfigReader Conf(ServerInstance);
		MySuffix = Conf.ReadValue("host","suffix",0);
		MyPrefix = Conf.ReadValue("host","prefix","",0);
		MySeparator = Conf.ReadValue("host","separator",".",0);
		for (hostchanges_t::iterator i = hostchanges.begin(); i != hostchanges.end(); i++)
		{
			DELETE(i->second);
		}
		hostchanges.clear();
		for (int index = 0; index < Conf.Enumerate("hostchange"); index++)
		{
			std::string mask = Conf.ReadValue("hostchange","mask",index);
			std::string action = Conf.ReadValue("hostchange","action",index);
			std::string newhost = Conf.ReadValue("hostchange","value",index);
			Host* x = new Host;
			x->action = action;
			x->newhost = newhost;
			hostchanges[mask] = x;
		}
	}
	
	virtual Version GetVersion()
	{
		// returns the version number of the module to be
		// listed in /MODULES
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
	
	virtual void OnUserConnect(userrec* user)
	{
		for (hostchanges_t::iterator i = hostchanges.begin(); i != hostchanges.end(); i++)
		{
			if (ServerInstance->MatchText(std::string(user->ident)+"@"+std::string(user->host),i->first))
			{
				Host* h = (Host*)i->second;
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
					user->WriteServ("NOTICE "+std::string(user->nick)+" :Setting your virtual host: " + newhost);
					if (!user->ChangeDisplayedHost(newhost.c_str()))
						user->WriteServ("NOTICE "+std::string(user->nick)+" :Could not set your virtual host: " + newhost);
					return;
				}
			}
		}
	}
};

MODULE_INIT(ModuleHostChange)
