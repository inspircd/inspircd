/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2018, 2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013, 2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004, 2006, 2008 Craig Edwards <brain@inspircd.org>
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
    // From UnrealIRCd.
    ERR_CANNOTKNOCK = 480,

    // From ircd-ratbox.
    RPL_KNOCK = 710,
    RPL_KNOCKDLVR = 711,
    ERR_CHANOPEN = 713,
    ERR_KNOCKONCHAN = 714
};

/** Handles the /KNOCK command
 */
class CommandKnock : public Command {
  private:
    SimpleChannelModeHandler& noknockmode;
    ChanModeReference inviteonlymode;
    Invite::API inviteapi;

  public:
    bool sendnotice;
    bool sendnumeric;
    CommandKnock(Module* Creator, SimpleChannelModeHandler& Noknockmode)
        : Command(Creator,"KNOCK", 2, 2)
        , noknockmode(Noknockmode)
        , inviteonlymode(Creator, "inviteonly")
        , inviteapi(Creator) {
        syntax = "<channel> :<reason>";
        Penalty = 5;
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        Channel* c = ServerInstance->FindChan(parameters[0]);
        if (!c) {
            user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
            return CMD_FAILURE;
        }

        if (c->HasUser(user)) {
            user->WriteNumeric(ERR_KNOCKONCHAN, c->name,
                               InspIRCd::Format("Can't KNOCK on %s, you are already on that channel.",
                                                c->name.c_str()));
            return CMD_FAILURE;
        }

        if (c->IsModeSet(noknockmode)) {
            user->WriteNumeric(ERR_CANNOTKNOCK,
                               InspIRCd::Format("Can't KNOCK on %s, +K is set.", c->name.c_str()));
            return CMD_FAILURE;
        }

        if (!c->IsModeSet(inviteonlymode)) {
            user->WriteNumeric(ERR_CHANOPEN, c->name,
                               InspIRCd::Format("Can't KNOCK on %s, channel is not invite only so knocking is pointless!",
                                                c->name.c_str()));
            return CMD_FAILURE;
        }

        // Work out who we should send the knock to.
        char status;
        switch (inviteapi->GetAnnounceState()) {
        case Invite::ANNOUNCE_ALL: {
            status = 0;
            break;
        }

        case Invite::ANNOUNCE_DYNAMIC: {
            PrefixMode* mh = ServerInstance->Modes.FindNearestPrefixMode(HALFOP_VALUE);
            status = mh->GetPrefix() ? mh->GetPrefix() : '@';
            break;
        }

        default: {
            status = '@';
            break;
        }
        }

        if (sendnotice) {
            c->WriteNotice(InspIRCd::Format("User %s is KNOCKing on %s (%s)",
                                            user->nick.c_str(), c->name.c_str(), parameters[1].c_str()), status);
            user->WriteNotice("KNOCKing on " + c->name);
        }

        if (sendnumeric) {
            Numeric::Numeric numeric(RPL_KNOCK);
            numeric.push(c->name).push(user->GetFullHost()).push("is KNOCKing: " +
                    parameters[1]);

            ClientProtocol::Messages::Numeric numericmsg(numeric, c->name);
            c->Write(ServerInstance->GetRFCEvents().numeric, numericmsg, status);

            user->WriteNumeric(RPL_KNOCKDLVR, c->name, "KNOCKing on channel");
        }

        return CMD_SUCCESS;
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        return ROUTE_OPT_BCAST;
    }
};

class ModuleKnock : public Module {
    SimpleChannelModeHandler kn;
    CommandKnock cmd;

  public:
    ModuleKnock()
        : kn(this, "noknock", 'K')
        , cmd(this, kn) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        std::string knocknotify = ServerInstance->Config->ConfValue("knock")->getString("notify");
        if (stdalgo::string::equalsci(knocknotify, "numeric")) {
            cmd.sendnotice = false;
            cmd.sendnumeric = true;
        } else if (stdalgo::string::equalsci(knocknotify, "both")) {
            cmd.sendnotice = true;
            cmd.sendnumeric = true;
        } else {
            cmd.sendnotice = true;
            cmd.sendnumeric = false;
        }
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /KNOCK command which allows users to request access to an invite-only channel and channel mode K (noknock) which allows channels to disable usage of this command.", VF_OPTCOMMON | VF_VENDOR);
    }
};

MODULE_INIT(ModuleKnock)
