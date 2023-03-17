/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 John Brooks <special@inspircd.org>
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

/** Handle /CYCLE
 */
class CommandCycle : public SplitCommand {
  public:
    CommandCycle(Module* Creator)
        : SplitCommand(Creator, "CYCLE", 1) {
        Penalty = 3;
        syntax = "<channel> [:<reason>]";
    }

    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        Channel* channel = ServerInstance->FindChan(parameters[0]);
        std::string reason = "Cycling";

        if (parameters.size() > 1) {
            /* reason provided, use it */
            reason = reason + ": " + parameters[1];
        }

        if (!channel) {
            user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
            return CMD_FAILURE;
        }

        if (channel->HasUser(user)) {
            if (channel->GetPrefixValue(user) < VOICE_VALUE && channel->IsBanned(user)) {
                // User is banned, send an error and don't cycle them
                user->WriteNotice("*** You may not cycle, as you are banned on channel " +
                                  channel->name);
                return CMD_FAILURE;
            }

            channel->PartUser(user, reason);
            Channel::JoinUser(user, parameters[0], true);

            return CMD_SUCCESS;
        } else {
            user->WriteNumeric(ERR_NOTONCHANNEL, channel->name,
                               "You're not on that channel");
        }

        return CMD_FAILURE;
    }
};


class ModuleCycle : public Module {
    CommandCycle cmd;

  public:
    ModuleCycle()
        : cmd(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows channel members to part and rejoin a channel without needing to worry about channel modes such as +i (inviteonly) which might prevent rejoining.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleCycle)
