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
#include "modules/httpd.h"

class ModuleHttpConfig : public Module, public HTTPRequestEventListener
{
	HTTPdAPI API;

 public:
	ModuleHttpConfig()
		: HTTPRequestEventListener(this)
		, API(this)
	{
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

	ModResult HandleRequest(HTTPRequest* http)
	{
		std::stringstream data("");

		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Handling httpd event");

			if ((http->GetURI() == "/config") || (http->GetURI() == "/config/"))
			{
				data << "<html><head><title>InspIRCd Configuration</title></head><body>";
				data << "<h1>InspIRCd Configuration</h1><p>";

				for (ConfigDataHash::iterator x = ServerInstance->Config->config_data.begin(); x != ServerInstance->Config->config_data.end(); ++x)
				{
					data << "&lt;" << x->first << " ";
					const ConfigItems& items = x->second->getItems();
					for (ConfigItems::const_iterator j = items.begin(); j != items.end(); j++)
					{
						data << Sanitize(j->first) << "=&quot;" << Sanitize(j->second) << "&quot; ";
					}
					data << "&gt;<br>";
				}

				data << "</body></html>";
				/* Send the document back to m_httpd */
				HTTPDocumentResponse response(this, *http, &data, 200);
				response.headers.SetHeader("X-Powered-By", MODNAME);
				response.headers.SetHeader("Content-Type", "text/html");
				API->SendResponse(response);
				return MOD_RES_DENY; // Handled
			}
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnHTTPRequest(HTTPRequest& req) CXX11_OVERRIDE
	{
		return HandleRequest(&req);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows for the server configuration to be viewed over HTTP via m_httpd.so", VF_VENDOR);
	}
};

MODULE_INIT(ModuleHttpConfig)
