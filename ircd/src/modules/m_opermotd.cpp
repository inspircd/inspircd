/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2005, 2007 Robin Burchell <robin+git@viroteck.net>
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

enum {
    // From UnrealIRCd.
    ERR_NOOPERMOTD = 425,

    // From ircd-ratbox.
    RPL_OMOTDSTART = 720,
    RPL_OMOTD = 721,
    RPL_ENDOFOMOTD = 722
};

/** Handle /OPERMOTD
 */
class CommandOpermotd : public Command {
  public:
    file_cache opermotd;

    CommandOpermotd(Module* Creator) : Command(Creator,"OPERMOTD", 0, 1) {
        flags_needed = 'o';
        syntax = "[<servername>]";
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        if ((parameters.empty()) || (irc::equals(parameters[0], ServerInstance->Config->ServerName))) {
            ShowOperMOTD(user, true);
        }
        return CMD_SUCCESS;
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        if ((!parameters.empty()) && (parameters[0].find('.') != std::string::npos)) {
            return ROUTE_OPT_UCAST(parameters[0]);
        }
        return ROUTE_LOCALONLY;
    }

    void ShowOperMOTD(User* user, bool show_missing) {
        if (opermotd.empty()) {
            if (show_missing) {
                user->WriteRemoteNumeric(ERR_NOOPERMOTD, "OPERMOTD file is missing.");
            }
            return;
        }

        user->WriteRemoteNumeric(RPL_OMOTDSTART, "Server operators message of the day");
        for (file_cache::const_iterator i = opermotd.begin(); i != opermotd.end();
                ++i) {
            user->WriteRemoteNumeric(RPL_OMOTD, *i);
        }
        user->WriteRemoteNumeric(RPL_ENDOFOMOTD, "End of OPERMOTD");
    }
};


class ModuleOpermotd : public Module {
    CommandOpermotd cmd;
    bool onoper;
  public:

    ModuleOpermotd()
        : cmd(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /OPERMOTD command which adds a special message of the day for server operators.", VF_VENDOR | VF_OPTCOMMON);
    }

    void OnOper(User* user, const std::string &opertype) CXX11_OVERRIDE {
        if (onoper && IS_LOCAL(user)) {
            cmd.ShowOperMOTD(user, false);
        }
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        cmd.opermotd.clear();
        ConfigTag* conf = ServerInstance->Config->ConfValue("opermotd");
        onoper = conf->getBool("onoper", true);

        try {
            FileReader reader(conf->getString("file", "opermotd", 1));

            const file_cache& lines = reader.GetVector();

            // Process the MOTD entry.
            cmd.opermotd.reserve(lines.size());
            for (file_cache::const_iterator it = lines.begin(); it != lines.end(); ++it) {
                // Some clients can not handle receiving RPL_OMOTD with an empty
                // trailing parameter so if a line is empty we replace it with
                // a single space.
                const std::string& line = *it;
                cmd.opermotd.push_back(line.empty() ? " " : line);
            }
            InspIRCd::ProcessColors(cmd.opermotd);
        } catch (CoreException&) {
            // Nothing happens here as we do the error handling in ShowOperMOTD.
        }
    }
};

MODULE_INIT(ModuleOpermotd)
