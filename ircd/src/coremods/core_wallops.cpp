/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

/** Handle /WALLOPS.
 */
class CommandWallops : public Command {
    SimpleUserModeHandler wallopsmode;
    ClientProtocol::EventProvider protoevprov;

  public:
    /** Constructor for wallops.
     */
    CommandWallops(Module* parent)
        : Command(parent, "WALLOPS", 1, 1)
        , wallopsmode(parent, "wallops", 'w')
        , protoevprov(parent, name) {
        flags_needed = 'o';
        syntax = ":<message>";
    }

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        return ROUTE_BROADCAST;
    }
};

CmdResult CommandWallops::Handle(User* user, const Params& parameters) {
    if (parameters[0].empty()) {
        user->WriteNumeric(ERR_NOTEXTTOSEND, "No text to send");
        return CMD_FAILURE;
    }

    ClientProtocol::Message msg("WALLOPS", user);
    msg.PushParamRef(parameters[0]);
    ClientProtocol::Event wallopsevent(protoevprov, msg);

    const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
    for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end();
            ++i) {
        LocalUser* curr = *i;
        if (curr->IsModeSet(wallopsmode)) {
            curr->Send(wallopsevent);
        }
    }

    return CMD_SUCCESS;
}

class CoreModWallops : public Module {
  private:
    CommandWallops cmd;

  public:
    CoreModWallops()
        : cmd(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the WALLOPS command", VF_CORE | VF_VENDOR);
    }
};

MODULE_INIT(CoreModWallops)
