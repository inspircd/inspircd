/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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
#include "httpd.h"
#include "protocol.h"

/* $ModDesc: Allows for the server configuration to be viewed over HTTP via m_httpd.so */

class ModuleHttpConfig : public Module
{
 public:
	void init()
	{
		Implementation eventlist[] = { I_OnEvent };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
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
				HTTPDocumentResponse response(this, *http, &data, 200);
				response.headers.SetHeader("X-Powered-By", "m_httpd_config.so");
				response.headers.SetHeader("Content-Type", "text/html");
				response.Send();
			}
		}
	}

	virtual ~ModuleHttpConfig()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Allows for the server configuration to be viewed over HTTP via m_httpd.so", VF_VENDOR);
	}
};

MODULE_INIT(ModuleHttpConfig)
