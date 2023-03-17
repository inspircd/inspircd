/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2006, 2008 Craig Edwards <brain@inspircd.org>
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
    // InspIRCd-specific.
    ERR_NICKNOTLOCKED = 946,
    RPL_NICKLOCKON = 947,
    RPL_NICKLOCKOFF = 945
};

/** Handle /NICKLOCK
 */
class CommandNicklock : public Command {
  public:
    LocalIntExt& locked;
    CommandNicklock (Module* Creator, LocalIntExt& ext) : Command(Creator,
                "NICKLOCK", 2),
        locked(ext) {
        flags_needed = 'o';
        syntax = "<nick> <newnick>";
        TRANSLATE2(TR_NICK, TR_TEXT);
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        User* target = ServerInstance->FindNick(parameters[0]);

        if ((!target) || (target->registered != REG_ALL)) {
            user->WriteNotice("*** No such nickname: '" + parameters[0] + "'");
            return CMD_FAILURE;
        }

        /* Do local sanity checks and bails */
        if (IS_LOCAL(user)) {
            if (!ServerInstance->IsNick(parameters[1])) {
                user->WriteNotice("*** Invalid nickname '" + parameters[1] + "'");
                return CMD_FAILURE;
            }

            user->WriteNumeric(RPL_NICKLOCKON, parameters[1], "Nickname now locked.");
        }

        /* If we made it this far, extend the user */
        if (IS_LOCAL(target)) {
            locked.set(target, 1);

            std::string oldnick = target->nick;
            if (target->ChangeNick(parameters[1])) {
                ServerInstance->SNO->WriteGlobalSno('a',
                                                    user->nick+" used NICKLOCK to change and hold "+oldnick+" to "+parameters[1]);
            } else {
                std::string newnick = target->nick;
                ServerInstance->SNO->WriteGlobalSno('a',
                                                    user->nick+" used NICKLOCK, but "+oldnick+" failed nick change to "
                                                    +parameters[1]+" and was locked to "+newnick+" instead");
            }
        }

        return CMD_SUCCESS;
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        return ROUTE_OPT_UCAST(parameters[0]);
    }
};

/** Handle /NICKUNLOCK
 */
class CommandNickunlock : public Command {
  public:
    LocalIntExt& locked;
    CommandNickunlock (Module* Creator, LocalIntExt& ext) : Command(Creator,
                "NICKUNLOCK", 1),
        locked(ext) {
        flags_needed = 'o';
        syntax = "<nick>";
        TRANSLATE1(TR_NICK);
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        User* target = ServerInstance->FindNick(parameters[0]);

        if (!target) {
            user->WriteNotice("*** No such nickname: '" + parameters[0] + "'");
            return CMD_FAILURE;
        }

        if (IS_LOCAL(target)) {
            if (locked.set(target, 0)) {
                ServerInstance->SNO->WriteGlobalSno('a',
                                                    user->nick+" used NICKUNLOCK on "+target->nick);
                user->WriteRemoteNumeric(RPL_NICKLOCKOFF, target->nick,
                                         "Nickname now unlocked.");
            } else {
                user->WriteRemoteNumeric(ERR_NICKNOTLOCKED, target->nick,
                                         "This user's nickname is not locked.");
                return CMD_FAILURE;
            }
        }

        return CMD_SUCCESS;
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        return ROUTE_OPT_UCAST(parameters[0]);
    }
};

class ModuleNickLock : public Module {
    LocalIntExt locked;
    CommandNicklock cmd1;
    CommandNickunlock cmd2;
  public:
    ModuleNickLock()
        : locked("nick_locked", ExtensionItem::EXT_USER, this)
        , cmd1(this, locked)
        , cmd2(this, locked) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /NICKLOCK command which allows server operators to change a user's nickname and prevent them from changing it again until they disconnect.", VF_OPTCOMMON | VF_VENDOR);
    }

    ModResult OnUserPreNick(LocalUser* user,
                            const std::string& newnick) CXX11_OVERRIDE {
        if (locked.get(user)) {
            user->WriteNumeric(ERR_CANTCHANGENICK,
                               "You cannot change your nickname (your nick is locked)");
            return MOD_RES_DENY;
        }
        return MOD_RES_PASSTHRU;
    }

    void Prioritize() CXX11_OVERRIDE {
        Module *nflood = ServerInstance->Modules->Find("m_nickflood.so");
        ServerInstance->Modules->SetPriority(this, I_OnUserPreNick, PRIORITY_BEFORE, nflood);
    }
};

MODULE_INIT(ModuleNickLock)
