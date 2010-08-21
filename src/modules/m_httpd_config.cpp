/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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

class ModuleHttpConfig : public Module
{
 public:
	void init() 	{
		Implementation eventlist[] = { I_OnEvent };
		ServerInstance->Modules->Attach(eventlist, this, 1);
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

	void OnEvent(Event& event)
	{
		std::stringstream data("");

		if (event.id == "httpd_url")
		{
			ServerInstance->Logs->Log("m_http_stats", DEBUG,"Handling httpd event");
			HTTPRequest* http = (HTTPRequest*)&event;

			if ((http->GetURI() == "/config") || (http->GetURI() == "/config/"))
			{
				data << "<html><head><title>InspIRCd Configuration</title></head><body>";
				data << "<h1>InspIRCd Configuration</h1><p>";

				for (ConfigDataHash::iterator x = ServerInstance->Config->config_data.begin(); x != ServerInstance->Config->config_data.end(); ++x)
				{
					data << "&lt;" << x->first << " ";
					ConfigTag* tag = x->second;
					for (std::vector<KeyVal>::const_iterator j = tag->getItems().begin(); j != tag->getItems().end(); j++)
					{
						data << Sanitize(j->first) << "=&quot;" << Sanitize(j->second) << "&quot; ";
					}
					data << "&gt;<br>";
				}

				data << "</body></html>";
				/* Send the document back to m_httpd */
				HTTPDocumentResponse response(&data, 200);
				response.headers.SetHeader("X-Powered-By", "m_httpd_config.so");
				response.headers.SetHeader("Content-Type", "text/html");
				http->Respond(response);
			}
		}
	}

	virtual ~ModuleHttpConfig()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides configuration over HTTP via m_httpd.so", VF_VENDOR);
	}
};

MODULE_INIT(ModuleHttpConfig)
