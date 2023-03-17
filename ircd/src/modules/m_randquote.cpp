/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2003, 2006-2008 Craig Edwards <brain@inspircd.org>
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

class ModuleRandQuote : public Module {
  private:
    std::string prefix;
    std::string suffix;
    std::vector<std::string> quotes;

  public:
    void init() CXX11_OVERRIDE {
        ConfigTag* conf = ServerInstance->Config->ConfValue("randquote");
        prefix = conf->getString("prefix");
        suffix = conf->getString("suffix");
        FileReader reader(conf->getString("file", "quotes", 1));
        quotes = reader.GetVector();
    }

    void OnUserConnect(LocalUser* user) CXX11_OVERRIDE {
        if (!quotes.empty()) {
            unsigned long random = ServerInstance->GenRandomInt(quotes.size());
            user->WriteNotice(prefix + quotes[random] + suffix);
        }
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows random quotes to be sent to users when they connect to the server.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleRandQuote)
