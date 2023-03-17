/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 jamie <jamie@e03df62e-2008-0410-955e-edbf42e46eb7>
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

/// $ModAuthor: Torben Hørup
/// $ModAuthorMail: torben@t-hoerup.dk
/// $ModDepends: core 3
/// $ModDesc: Adds /SAMOVE command to move a user from one channel to another (basically combining SAPART+SAJOIN)

#include "inspircd.h"

/** Handle /SAMOVE
 *
 * Basically it's a SAPART + SAJOIN in 1 command
 */
class CommandSamove : public Command {
  public:
    CommandSamove(Module* Creator) : Command(Creator,"SAMOVE", 1) {
        allow_empty_last_param = false;
        flags_needed = 'o';
        syntax = "<nick> <fromchannel> <tochannel>";
        TRANSLATE3(TR_NICK, TR_TEXT, TR_TEXT);
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        if (parameters.size() != 3) {
            return CMD_FAILURE;
        }

        const std::string& nickname = parameters[0];
        const std::string& from_channel = parameters[1];
        const std::string& to_channel = parameters[2];

        User* dest = ServerInstance->FindNick(nickname);
        if ((dest) && (dest->registered == REG_ALL)) {

            if (dest->server->IsULine()) {
                user->WriteNumeric(ERR_NOPRIVILEGES,
                                   "Cannot use an SA command on a U-lined client");
                return CMD_FAILURE;
            }
            if (IS_LOCAL(user) && !ServerInstance->IsChannel(from_channel)) {
                /* we didn't need to check this for each character ;) */
                user->WriteNotice("*** Invalid characters in 'from channel' name or name too long");
                return CMD_FAILURE;
            }
            if (IS_LOCAL(user) && !ServerInstance->IsChannel(to_channel)) {
                /* we didn't need to check this for each character ;) */
                user->WriteNotice("*** Invalid characters in 'to channel' name or name too long");
                return CMD_FAILURE;
            }
            Channel* from_chan = ServerInstance->FindChan(from_channel);
            if (!(from_chan)) {
                user->WriteRemoteNotice("*** invalid 'from channel' " + from_channel);
                return CMD_FAILURE;
            }
            Channel* to_chan = ServerInstance->FindChan(to_channel);
            if (!(to_chan)) {
                user->WriteRemoteNotice("*** invalid 'to channel' " + to_channel);
                return CMD_FAILURE;
            }





            /* For local users, we call Channel::JoinUser which may create a channel and set its TS, also PART them directly
             * For non-local users, we just return CMD_SUCCESS, knowing this will propagate it where it needs to be
             * and then that server will handle the command.
             */
            LocalUser* localuser = IS_LOCAL(dest);
            if (localuser) {
                /*
                 *
                 */

                if ((from_chan) && (from_chan->HasUser(dest))) {
                    std::string msg; //PartUser doesn't accept a const reference atm
                    from_chan->PartUser(dest, msg);
                }

                if ((to_chan) && (!to_chan->HasUser(dest))) {
                    Channel* chan = Channel::JoinUser(localuser, to_channel, true);
                    if (!chan) {
                        user->WriteNotice("*** Could not join "+dest->nick+" to "+to_channel);
                        return CMD_FAILURE;
                    }

                }


                ServerInstance->SNO->WriteGlobalSno('m',
                                                    user->nick+" used SAMOVE to move "+dest->nick+" from "+from_channel+" to "
                                                    +to_channel);
                return CMD_SUCCESS;

            } else {
                return CMD_SUCCESS;
            }
        } else {
            user->WriteNotice("*** No such nickname: '" + nickname + "'");
            return CMD_FAILURE;
        }
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        return ROUTE_OPT_UCAST(parameters[0]);
    }
};

class ModuleSamove : public Module {
    CommandSamove cmd;
  public:
    ModuleSamove()
        : cmd(this) {
    }

    void init() CXX11_OVERRIDE {
        ServerInstance->SNO->EnableSnomask('m', "SAMOVE");
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /SAMOVE command which allows server operators to force move users from one channel to another.", VF_OPTCOMMON);
    }
};

MODULE_INIT(ModuleSamove)
