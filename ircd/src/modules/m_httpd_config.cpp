/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2018, 2020 Sadie Powell <sadie@witchery.services>
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

class ModuleHttpConfig : public Module, public HTTPRequestEventListener {
    HTTPdAPI API;

  public:
    ModuleHttpConfig()
        : HTTPRequestEventListener(this)
        , API(this) {
    }

    ModResult OnHTTPRequest(HTTPRequest& request) CXX11_OVERRIDE {
        if (request.GetPath() != "/config") {
            return MOD_RES_PASSTHRU;
        }

        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Handling HTTP request for %s", request.GetPath().c_str());
        std::stringstream buffer;

        ConfigDataHash& config = ServerInstance->Config->config_data;
        for (ConfigDataHash::const_iterator citer = config.begin(); citer != config.end(); ++citer) {
            // Show the location of the tag in a comment.
            ConfigTag* tag = citer->second;
            buffer << "# " << tag->getTagLocation() << std::endl
                   << '<' << tag->tag << ' ';

            // Print out the tag with all keys aligned vertically.
            const std::string indent(tag->tag.length() + 2, ' ');
            const ConfigItems& items = tag->getItems();
            for (ConfigItems::const_iterator kiter = items.begin(); kiter != items.end();
                ) {
                ConfigItems::const_iterator curr = kiter++;
                buffer << curr->first << "=\"" << ServerConfig::Escape(curr->second) << '"';
                if (kiter != items.end()) {
                    buffer << std::endl << indent;
                }
            }
            buffer << '>' << std::endl << std::endl;
        }

        HTTPDocumentResponse response(this, request, &buffer, 200);
        response.headers.SetHeader("X-Powered-By", MODNAME);
        response.headers.SetHeader("Content-Type", "text/plain");
        API->SendResponse(response);
        return MOD_RES_DENY;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows the server configuration to be viewed over HTTP via the /config path.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleHttpConfig)
