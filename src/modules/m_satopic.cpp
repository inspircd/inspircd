/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Oliver Lupton <om@inspircd.org>
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

/** Handle /SATOPIC
 */
class CommandSATopic : public Command {
  public:
    CommandSATopic(Module* Creator) : Command(Creator,"SATOPIC", 2, 2) {
        flags_needed = 'o';
        syntax = "<channel> :<topic>";
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        /*
         * Handles a SATOPIC request. Notifies all +s users.
         */
        Channel* target = ServerInstance->FindChan(parameters[0]);

        if(target) {
            const std::string newTopic(parameters[1], 0,
                                       ServerInstance->Config->Limits.MaxTopic);
            if (target->topic == newTopic) {
                user->WriteNotice(
                    InspIRCd::Format("The topic on %s is already what you are trying to change it to.",
                                     target->name.c_str()));
                return CMD_SUCCESS;
            }

            target->SetTopic(user, newTopic, ServerInstance->Time(), NULL);
            ServerInstance->SNO->WriteGlobalSno('a',
                                                user->nick + " used SATOPIC on " + target->name + ", new topic: " + newTopic);

            return CMD_SUCCESS;
        } else {
            user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
            return CMD_FAILURE;
        }
    }
};

class ModuleSATopic : public Module {
    CommandSATopic cmd;
  public:
    ModuleSATopic()
        : cmd(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /SATOPIC command which allows server operators to change the topic of a channel that they would not otherwise have the privileges to change.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleSATopic)
