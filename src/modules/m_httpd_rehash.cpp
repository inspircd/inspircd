/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
public:
	void init()
	{
		Implementation eventlist[] = { I_OnEvent };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
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
					ServerInstance->SNO->WriteToSnoMask('a', "Rehashing config file %s from HTTP request from %s", ServerConfig::CleanFilename(ServerInstance->ConfigFileName.c_str()), http->GetIP().c_str());
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

MODULE_INIT(ModuleHttpRehash)
