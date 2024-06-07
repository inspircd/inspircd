/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2021, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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

class ModuleHttpConfig final
	: public Module
	, public HTTPRequestEventListener
{
private:
	HTTPdAPI API;

public:
	ModuleHttpConfig()
		: Module(VF_VENDOR, "Allows the server configuration to be viewed over HTTP via the /config path.")
		, HTTPRequestEventListener(this)
		, API(this)
	{
	}

	ModResult OnHTTPRequest(HTTPRequest& request) override
	{
		if (request.GetPath() != "/config")
			return MOD_RES_PASSTHRU;

		ServerInstance->Logs.Debug(MODNAME, "Handling HTTP request for {}", request.GetPath());

		std::stringstream buffer;
		for (const auto& [_, tag] : ServerInstance->Config->GetConfig())
		{
			// Show the location of the tag in a comment.
			buffer << "# " << tag->source.str() << std::endl
				<< '<' << tag->name << ' ';

			// Print out the tag with all keys aligned vertically.
			const std::string indent(tag->name.length() + 2, ' ');
			bool first = true;
			for (const auto& [key, value] : tag->GetItems())
			{
				if (!first)
					buffer << std::endl << indent;

				buffer << key << "=\"" << ServerConfig::Escape(value) << '"';
				first = false;
			}
			buffer << '>' << std::endl << std::endl;
		}

		HTTPDocumentResponse response(this, request, &buffer, 200);
		response.headers.SetHeader("X-Powered-By", MODNAME);
		response.headers.SetHeader("Content-Type", "text/plain");
		API->SendResponse(response);
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleHttpConfig)
