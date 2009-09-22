/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "httpd.h"
#include "protocol.h"

/* $ModDesc: Provides statistics over HTTP via m_httpd.so */
/* $ModDep: httpd.h */

class ModuleHttpStats : public Module
{

	std::string stylesheet;
	bool changed;

 public:

	void ReadConfig()
	{
		ConfigReader c(ServerInstance);
		this->stylesheet = c.ReadValue("httpstats", "stylesheet", 0);
	}

	ModuleHttpStats(InspIRCd* Me) : Module(Me)
	{
		ReadConfig();
		this->changed = true;
		Implementation eventlist[] = { I_OnEvent, I_OnRequest };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	std::string Sanitize(const std::string &str)
	{
		std::string ret;

		for (std::string::const_iterator x = str.begin(); x != str.end(); ++x)
		{
			switch (*x)
			{
				case '<':
					ret += "&lt;";
				break;
				case '>':
					ret += "&gt;";
				break;
				case '&':
					ret += "&amp;";
				break;
				case '"':
					ret += "&quot;";
				break;
				default:
					if (*x < 32 || *x > 126)
					{
						int n = *x;
						ret += ("&#" + ConvToStr(n) + ";");
					}
					else
						ret += *x;
				break;
			}
		}
		return ret;
	}

	void OnEvent(Event* event)
	{
		std::stringstream data("");

		if (event->GetEventID() == "httpd_url")
		{
			ServerInstance->Logs->Log("m_http_stats", DEBUG,"Handling httpd event");
			HTTPRequest* http = (HTTPRequest*)event->GetData();

			if ((http->GetURI() == "/config") || (http->GetURI() == "/config/"))
			{
				data << "<html><head><title>InspIRCd Configuration</title></head><body>";
				data << "<h1>InspIRCd Configuration</h1><p>";

				for (ConfigDataHash::iterator x = ServerInstance->Config->config_data.begin(); x != ServerInstance->Config->config_data.end(); ++x)
				{
					data << "&lt;" << x->first << " ";
					for (KeyValList::iterator j = x->second.begin(); j != x->second.end(); j++)
					{
						data << j->first << "=&quot;" << j->second << "&quot; ";
					}
					data << "&gt;<br>";
				}

				data << "</body></html>";
				/* Send the document back to m_httpd */
				HTTPDocument response(http->sock, &data, 200);
				response.headers.SetHeader("X-Powered-By", "m_httpd_config.so");
				response.headers.SetHeader("Content-Type", "text/html");
				Request req((char*)&response, (Module*)this, event->GetSource());
				req.Send();
			}
		}
	}

	const char* OnRequest(Request* request)
	{
		return NULL;
	}


	virtual ~ModuleHttpStats()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides statistics over HTTP via m_httpd.so", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleHttpStats)
