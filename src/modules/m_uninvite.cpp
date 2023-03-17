/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2018-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
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
#include "modules/invite.h"

enum {
    // InspIRCd-specific.
    ERR_INVITEREMOVED = 494,
    ERR_NOTINVITED = 505,
    RPL_UNINVITED = 653
};

/** Handle /UNINVITE
 */
class CommandUninvite : public Command {
    Invite::API invapi;
  public:
    CommandUninvite(Module* Creator)
        : Command(Creator, "UNINVITE", 2)
        , invapi(Creator) {
        syntax = "<nick> <channel>";
        TRANSLATE2(TR_NICK, TR_TEXT);
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        User* u;
        if (IS_LOCAL(user)) {
            u = ServerInstance->FindNickOnly(parameters[0]);
        } else {
            u = ServerInstance->FindNick(parameters[0]);
        }

        Channel* c = ServerInstance->FindChan(parameters[1]);

        if ((!c) || (!u) || (u->registered != REG_ALL)) {
            if (!c) {
                user->WriteNumeric(Numerics::NoSuchChannel(parameters[1]));
            } else {
                user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
            }

            return CMD_FAILURE;
        }

        if (IS_LOCAL(user)) {
            if (c->GetPrefixValue(user) < HALFOP_VALUE) {
                user->WriteNumeric(Numerics::ChannelPrivilegesNeeded(c, HALFOP_VALUE,
                                   "remove an invite"));
                return CMD_FAILURE;
            }
        }

        /* Servers remember invites only for their local users, so act
         * only if the target is local. Otherwise the command will be
         * passed to the target users server.
         */
        LocalUser* lu = IS_LOCAL(u);
        if (lu) {
            // XXX: The source of the numeric we send must be the server of the user doing the /UNINVITE,
            // so they don't see where the target user is connected to
            if (!invapi->Remove(lu, c)) {
                Numeric::Numeric n(ERR_NOTINVITED);
                n.SetServer(user->server);
                n.push(u->nick).push(c->name).push(
                    InspIRCd::Format("Is not invited to channel %s", c->name.c_str()));
                user->WriteRemoteNumeric(n);
                return CMD_FAILURE;
            }

            Numeric::Numeric n(ERR_INVITEREMOVED);
            n.SetServer(user->server);
            n.push(c->name).push(u->nick).push("Uninvited");
            user->WriteRemoteNumeric(n);

            lu->WriteNumeric(RPL_UNINVITED,
                             InspIRCd::Format("You were uninvited from %s by %s", c->name.c_str(),
                                              user->nick.c_str()));
            c->WriteRemoteNotice(InspIRCd::Format("*** %s uninvited %s.",
                                                  user->nick.c_str(), u->nick.c_str()));
        }

        return CMD_SUCCESS;
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        return ROUTE_OPT_UCAST(parameters[0]);
    }
};

class ModuleUninvite : public Module {
    CommandUninvite cmd;

  public:

    ModuleUninvite() : cmd(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /UNINVITE command which allows users who have invited another user to a channel to withdraw their invite.", VF_VENDOR | VF_OPTCOMMON);
    }
};

MODULE_INIT(ModuleUninvite)
