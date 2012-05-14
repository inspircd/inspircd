/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2012 Chin Lee <kwangchin@gmail.com>
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
#include "threadengine.h"
#include "httpd.h"
#include "protocol.h"

class ModuleHttpRehash : public Module
{
	static std::map<char, char const*> const &entities;

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
			ServerInstance->Logs->Log("m_http_stats", DEBUG,"Handling httpd rehash event");
			HTTPRequest* http = (HTTPRequest*)&event;

			if ((http->GetURI() == "/rehash") || (http->GetURI() == "/rehash/"))
			{
				if (!ServerInstance->PendingRehash)
				{
					data << "Rehashed";
					ServerInstance->SNO->WriteToSnoMask('a', "Rehashing config file %s from Admin Panel",ServerConfig::CleanFilename(ServerInstance->ConfigFileName.c_str()));
					ServerInstance->PendingRehash = new ConfigReaderThread("");
					ServerInstance->Threads->Submit(ServerInstance->PendingRehash);
				} else {
					data << "Rehashing";
				}

				/* Send the document back to m_httpd */
				HTTPDocumentResponse response(&data, 200);
				response.headers.SetHeader("X-Powered-By", "m_httpd_rehash.so");
				response.headers.SetHeader("Content-Type", "text/html");
				http->Respond(response);
			}
		}
	}

	virtual ~ModuleHttpRehash()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides rehash over HTTP via m_httpd.so", VF_VENDOR);
	}
};

static std::map<char, char const*> const &init_entities()
{
	static std::map<char, char const*> entities;
	entities['<'] = "lt";
	entities['>'] = "gt";
	entities['&'] = "amp";
	entities['"'] = "quot";
	return entities;
}

std::map<char, char const*> const &ModuleHttpRehash::entities = init_entities ();

MODULE_INIT(ModuleHttpRehash)
