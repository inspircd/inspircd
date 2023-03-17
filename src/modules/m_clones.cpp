/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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
#include "modules/ircv3_batch.h"

enum {
    // InspIRCd-specific.
    RPL_CLONES = 399
};

class CommandClones : public SplitCommand {
  private:
    IRCv3::Batch::API batchmanager;
    IRCv3::Batch::Batch batch;

  public:
    CommandClones(Module* Creator)
        : SplitCommand(Creator,"CLONES", 1)
        , batchmanager(Creator)
        , batch("inspircd.org/clones") {
        flags_needed = 'o';
        syntax = "<limit>";
    }

    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        unsigned int limit = ConvToNum<unsigned int>(parameters[0]);

        // Syntax of a CLONES reply:
        // :irc.example.com BATCH +<id> inspircd.org/clones :<min-count>
        // @batch=<id> :irc.example.com 399 <client> <local-count> <remote-count> <cidr-mask>
        /// :irc.example.com BATCH :-<id>

        if (batchmanager) {
            batchmanager->Start(batch);
            batch.GetBatchStartMessage().PushParam(ConvToStr(limit));
        }

        const UserManager::CloneMap& clonemap = ServerInstance->Users->GetCloneMap();
        for (UserManager::CloneMap::const_iterator i = clonemap.begin(); i != clonemap.end(); ++i) {
            const UserManager::CloneCounts& counts = i->second;
            if (counts.global < limit) {
                continue;
            }

            Numeric::Numeric numeric(RPL_CLONES);
            numeric.push(counts.local);
            numeric.push(counts.global);
            numeric.push(i->first.str());

            ClientProtocol::Messages::Numeric numericmsg(numeric, user);
            batch.AddToBatch(numericmsg);
            user->Send(ServerInstance->GetRFCEvents().numeric, numericmsg);
        }

        if (batchmanager) {
            batchmanager->End(batch);
        }

        return CMD_SUCCESS;
    }
};

class ModuleClones : public Module {
  public:
    CommandClones cmd;

  public:
    ModuleClones()
        : cmd(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /CLONES command which allows server operators to view IP addresses from which there are more than a specified number of connections.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleClones)
