/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2017-2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

/** Handle /LOADMODULE.
 */
class CommandLoadmodule : public Command {
  public:
    /** Constructor for loadmodule.
     */
    CommandLoadmodule(Module* parent)
        : Command(parent,"LOADMODULE", 1, 1) {
        flags_needed = 'o';
        syntax = "<modulename>";
    }

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /LOADMODULE
 */
CmdResult CommandLoadmodule::Handle(User* user, const Params& parameters) {
    if (ServerInstance->Modules->Load(parameters[0])) {
        ServerInstance->SNO->WriteGlobalSno('a', "NEW MODULE: %s loaded %s",
                                            user->nick.c_str(), parameters[0].c_str());
        user->WriteNumeric(RPL_LOADEDMODULE, parameters[0],
                           "Module successfully loaded.");
        return CMD_SUCCESS;
    } else {
        user->WriteNumeric(ERR_CANTLOADMODULE, parameters[0],
                           ServerInstance->Modules->LastError());
        return CMD_FAILURE;
    }
}

/** Handle /UNLOADMODULE.
 */
class CommandUnloadmodule : public Command {
  public:
    bool allowcoreunload;

    /** Constructor for unloadmodule.
     */
    CommandUnloadmodule(Module* parent)
        : Command(parent, "UNLOADMODULE", 1)
        , allowcoreunload(false) {
        flags_needed = 'o';
        syntax = "<modulename>";
    }

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

CmdResult CommandUnloadmodule::Handle(User* user, const Params& parameters) {
    if (!allowcoreunload
            && InspIRCd::Match(parameters[0], "core_*.so", ascii_case_insensitive_map)) {
        user->WriteNumeric(ERR_CANTUNLOADMODULE, parameters[0],
                           "You cannot unload core commands!");
        return CMD_FAILURE;
    }

    Module* m = ServerInstance->Modules->Find(parameters[0]);
    if (m == creator) {
        user->WriteNumeric(ERR_CANTUNLOADMODULE, parameters[0],
                           "You cannot unload module loading commands!");
        return CMD_FAILURE;
    }

    if (m && ServerInstance->Modules->Unload(m)) {
        ServerInstance->SNO->WriteGlobalSno('a', "MODULE UNLOADED: %s unloaded %s",
                                            user->nick.c_str(), parameters[0].c_str());
        user->WriteNumeric(RPL_UNLOADEDMODULE, parameters[0],
                           "Module successfully unloaded.");
    } else {
        user->WriteNumeric(ERR_CANTUNLOADMODULE, parameters[0],
                           (m ? ServerInstance->Modules->LastError() : "No such module"));
        return CMD_FAILURE;
    }

    return CMD_SUCCESS;
}

class CoreModLoadModule : public Module {
    CommandLoadmodule cmdloadmod;
    CommandUnloadmodule cmdunloadmod;

  public:
    CoreModLoadModule()
        : cmdloadmod(this), cmdunloadmod(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the LOADMODULE and UNLOADMODULE commands", VF_VENDOR|VF_CORE);
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("security");
        cmdunloadmod.allowcoreunload = tag->getBool("allowcoreunload");
    }
};

MODULE_INIT(CoreModLoadModule)
