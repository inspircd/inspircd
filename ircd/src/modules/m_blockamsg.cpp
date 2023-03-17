/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
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

enum BlockAction { IBLOCK_KILL, IBLOCK_KILLOPERS, IBLOCK_NOTICE, IBLOCK_NOTICEOPERS, IBLOCK_SILENT };
/*  IBLOCK_NOTICE       - Send a notice to the user informing them of what happened.
 *  IBLOCK_NOTICEOPERS  - Send a notice to the user informing them and send an oper notice.
 *  IBLOCK_SILENT       - Generate no output, silently drop messages.
 *  IBLOCK_KILL         - Kill the user with the reason "Global message (/amsg or /ame) detected".
 *  IBLOCK_KILLOPERS    - As above, but send an oper notice as well. This is the default.
 */

/** Holds a blocked message's details
 */
class BlockedMessage {
  public:
    std::string message;
    std::string target;
    time_t sent;

    BlockedMessage(const std::string& msg, const std::string& tgt, time_t when)
        : message(msg), target(tgt), sent(when) {
    }
};

class ModuleBlockAmsg : public Module {
    unsigned int ForgetDelay;
    BlockAction action;
    SimpleExtItem<BlockedMessage> blockamsg;

  public:
    ModuleBlockAmsg()
        : blockamsg("blockamsg", ExtensionItem::EXT_USER, this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Blocks mass messages sent using the /AME and /AMSG commands that exist in clients such as mIRC and HexChat.", VF_VENDOR);
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("blockamsg");
        ForgetDelay = tag->getDuration("delay", 3);
        std::string act = tag->getString("action");

        if (stdalgo::string::equalsci(act, "notice")) {
            action = IBLOCK_NOTICE;
        } else if (stdalgo::string::equalsci(act, "noticeopers")) {
            action = IBLOCK_NOTICEOPERS;
        } else if (stdalgo::string::equalsci(act, "silent")) {
            action = IBLOCK_SILENT;
        } else if (stdalgo::string::equalsci(act, "kill")) {
            action = IBLOCK_KILL;
        } else {
            action = IBLOCK_KILLOPERS;
        }
    }

    ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters,
                           LocalUser* user, bool validated) CXX11_OVERRIDE {
        // Don't do anything with unregistered users
        if (user->registered != REG_ALL) {
            return MOD_RES_PASSTHRU;
        }

        if ((validated) && (parameters.size() >= 2) && ((command == "PRIVMSG") || (command == "NOTICE"))) {
            if (user->HasPrivPermission("servers/ignore-blockamsg")) {
                return MOD_RES_PASSTHRU;
            }

            // parameters[0] is the target list, count how many channels are there
            unsigned int targets = 0;
            // Is the first target a channel?
            if (*parameters[0].c_str() == '#') {
                targets = 1;
            }

            for (const char* c = parameters[0].c_str(); *c; c++) {
                if ((*c == ',') && (*(c+1) == '#')) {
                    targets++;
                }
            }

            /* targets should now contain the number of channel targets the msg/notice was pointed at.
             * If the msg/notice was a PM there should be no channel targets and 'targets' should = 0.
             * We don't want to block PMs so...
             */
            if (targets == 0) {
                return MOD_RES_PASSTHRU;
            }

            // Check that this message wasn't already sent within a few seconds.
            BlockedMessage* m = blockamsg.get(user);

            // If the message is identical and within the time.
            // We check the target is *not* identical, that'd straying into the realms of flood control. Which isn't what we're doing...
            // OR
            // The number of target channels is equal to the number of channels the sender is on..a little suspicious.
            // Check it's more than 1 too, or else users on one channel would have fun.
            if ((m && (m->message == parameters[1])
                    && (!irc::equals(m->target, parameters[0])) && ForgetDelay
                    && (m->sent >= ServerInstance->Time()-(time_t)ForgetDelay)) || ((targets > 1)
                            && (targets == user->chans.size()))) {
                // Block it...
                if (action == IBLOCK_KILLOPERS || action == IBLOCK_NOTICEOPERS) {
                    ServerInstance->SNO->WriteToSnoMask('a', "%s had an /amsg or /ame blocked",
                                                        user->nick.c_str());
                }

                if (action == IBLOCK_KILL || action == IBLOCK_KILLOPERS) {
                    ServerInstance->Users->QuitUser(user,
                                                    "Attempted to global message (/amsg or /ame)");
                } else if (action == IBLOCK_NOTICE || action == IBLOCK_NOTICEOPERS) {
                    user->WriteNotice("Global message (/amsg or /ame) blocked");
                }

                return MOD_RES_DENY;
            }

            if (m) {
                // If there's already a BlockedMessage allocated, use it.
                m->message = parameters[1];
                m->target = parameters[0];
                m->sent = ServerInstance->Time();
            } else {
                m = new BlockedMessage(parameters[1], parameters[0], ServerInstance->Time());
                blockamsg.set(user, m);
            }
        }
        return MOD_RES_PASSTHRU;
    }
};

MODULE_INIT(ModuleBlockAmsg)
