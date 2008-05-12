/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "httpd.h"
#include "protocol.h"
#include "wildcard.h"

/* $ModDesc: Provides access control lists (passwording of resources, ip restrictions etc) to m_httpd.so dependent modules */
/* $ModDep: httpd.h */

class ACL : public Extensible
{
 public:
	std::string path;
	std::string password;
	std::string whitelist;
	std::string blacklist;

	ACL(const std::string &set_path, const std::string &set_password,
		const std::string &set_whitelist, const std::string &set_blacklist)
		: path(set_path), password(set_password), whitelist(set_whitelist),
		blacklist(set_blacklist) { }

	~ACL() { }
};

class ModuleHTTPAccessList : public Module
{
	
	std::string stylesheet;
	bool changed;
	std::vector<ACL> acl_list;

 public:

	void ReadConfig()
	{
		acl_list.clear();
		ConfigReader c(ServerInstance);
		int n_items = c.Enumerate("httpacl");
		for (int i = 0; i < n_items; ++i)
		{
			std::string path = c.ReadValue("httpacl", "path", i);
			std::string types = c.ReadValue("httpacl", "types", i);
			irc::commasepstream sep(types);
			std::string type;
			std::string password;
			std::string whitelist;
			std::string blacklist;

			while (sep.GetToken(type))
			{
				if (type == "password")
				{
					password = c.ReadValue("httpacl", "password", i);
				}
				else if (type == "whitelist")
				{
					whitelist = c.ReadValue("httpacl", "whitelist", i);
				}
				else if (type == "blacklist")
				{
					blacklist = c.ReadValue("httpacl", "blacklist", i);
				}
				else
				{
					throw ModuleException("Invalid HTTP ACL type '" + type + "'");
				}
			}

			acl_list.push_back(ACL(path, password, whitelist, blacklist));
		}
	}

	ModuleHTTPAccessList(InspIRCd* Me) : Module(Me)
	{
		ReadConfig();
		this->changed = true;
		Implementation eventlist[] = { I_OnEvent, I_OnRequest };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	void OnEvent(Event* event)
	{
		std::stringstream data("");

		if (event->GetEventID() == "httpd_url")
		{
			ServerInstance->Logs->Log("m_http_stats", DEBUG,"Handling httpd event");
			HTTPRequest* http = (HTTPRequest*)event->GetData();

			for (std::vector<ACL>::const_iterator this_acl = acl_list.begin(); this_acl != acl_list.end(); ++this_acl)
			{
				if (match(http->GetURI(), this_acl->path))
				{
					if (!this_acl->blacklist.empty())
					{
						/* Blacklist */
					}
					if (!this_acl->whitelist.empty())
					{
						/* Whitelist */
					}
					if (!this_acl->password.empty())
					{
						/* Password auth */
					}
				}
			}

			//if ((http->GetURI() == "/stats") || (http->GetURI() == "/stats/"))
			//{
				/* Send the document back to m_httpd */
			//	HTTPDocument response(http->sock, &data, 200);
			//	response.headers.SetHeader("X-Powered-By", "m_httpd_stats.so");
			//	response.headers.SetHeader("Content-Type", "text/xml");
			//	Request req((char*)&response, (Module*)this, event->GetSource());
			//	req.Send();
			//}
		}
	}

	const char* OnRequest(Request* request)
	{
		return NULL;
	}

	virtual ~ModuleHTTPAccessList()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1, 2, 0, 0, VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleHTTPAccessList)
