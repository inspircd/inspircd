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

/* $ModDesc: Provides access control lists (passwording of resources, ip restrictions etc) to m_httpd.so dependent modules */
/* $ModDep: httpd.h */

class ModuleHTTPAccessList : public Module
{
	
	std::string stylesheet;
	bool changed;

 public:

	void ReadConfig()
	{
		ConfigReader c(ServerInstance);
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
