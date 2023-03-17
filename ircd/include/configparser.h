/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2016, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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


#pragma once

struct ParseStack {
    std::vector<std::string> reading;
    insp::flat_map<std::string, std::string, irc::insensitive_swo> vars;
    ConfigDataHash& output;
    ConfigFileCache& FilesOutput;
    std::stringstream& errstr;

    ParseStack(ServerConfig* conf);
    bool ParseFile(const std::string& name, int flags,
                   const std::string& mandatory_tag = std::string(), bool isexec = false);
    void DoInclude(ConfigTag* includeTag, int flags);
    void DoReadFile(const std::string& key, const std::string& file, int flags,
                    bool exec);
};
