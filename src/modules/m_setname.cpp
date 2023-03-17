/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 delthas
 *   Copyright (C) 2013, 2018-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 John Brooks <special@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004 Craig Edwards <brain@inspircd.org>
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
#include "modules/ircv3.h"
#include "modules/ircv3_replies.h"
#include "modules/monitor.h"

class CommandSetName : public SplitCommand {
  private:
    IRCv3::Replies::Fail fail;

  public:
    Cap::Capability cap;
    bool notifyopers;

    CommandSetName(Module* Creator)
        : SplitCommand(Creator, "SETNAME", 1, 1)
        , fail(Creator)
        , cap(Creator, "setname") {
        allow_empty_last_param = false;
        syntax = ":<realname>";
    }

    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        if (parameters[0].size() > ServerInstance->Config->Limits.MaxReal) {
            fail.SendIfCap(user, cap, this, "INVALID_REALNAME", "Real name is too long");
            return CMD_FAILURE;
        }

        if (!user->ChangeRealName(parameters[0])) {
            fail.SendIfCap(user, cap, this, "CANNOT_CHANGE_REALNAME",
                           "Unable to change your real name");
            return CMD_FAILURE;
        }

        if (notifyopers)
            ServerInstance->SNO->WriteGlobalSno('a', "%s used SETNAME to change their real name to '%s'",
                                                user->nick.c_str(), parameters[0].c_str());
        return CMD_SUCCESS;
    }
};

class ModuleSetName : public Module {
  private:
    CommandSetName cmd;
    ClientProtocol::EventProvider setnameevprov;
    Monitor::API monitorapi;

  public:
    ModuleSetName()
        : cmd(this)
        , setnameevprov(this, "SETNAME")
        , monitorapi(this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("setname");

        // Whether the module should only be usable by server operators.
        bool operonly = tag->getBool("operonly");
        cmd.flags_needed = operonly ? 'o' : 0;

        // Whether a snotice should be sent out when a user changes their real name.
        cmd.notifyopers = tag->getBool("notifyopers", !operonly);
    }

    void OnChangeRealName(User* user, const std::string& real) CXX11_OVERRIDE {
        if (!(user->registered & REG_NICKUSER)) {
            return;
        }

        ClientProtocol::Message msg("SETNAME", user);
        msg.PushParamRef(real);
        ClientProtocol::Event protoev(setnameevprov, msg);
        IRCv3::WriteNeighborsWithCap res(user, protoev, cmd.cap, true);
        Monitor::WriteWatchersWithCap(monitorapi, user, protoev, cmd.cap, res.GetAlreadySentId());
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /SETNAME command which allows users to change their real name (gecos).", VF_VENDOR);
    }
};

MODULE_INIT(ModuleSetName)
