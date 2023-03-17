/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
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

CommandBase::CommandBase(Module* mod, const std::string& cmd,
                         unsigned int minpara, unsigned int maxpara)
    : ServiceProvider(mod, cmd, SERVICE_COMMAND)
    , min_params(minpara)
    , max_params(maxpara)
    , allow_empty_last_param(true) {
}

CommandBase::~CommandBase() {
}

void CommandBase::EncodeParameter(std::string& parameter, unsigned int index) {
}

RouteDescriptor CommandBase::GetRouting(User* user, const Params& parameters) {
    return ROUTE_LOCALONLY;
}

Command::Command(Module* mod, const std::string& cmd, unsigned int minpara,
                 unsigned int maxpara)
    : CommandBase(mod, cmd, minpara, maxpara)
    , flags_needed(0)
    , force_manual_route(false)
    , Penalty(1)
    , use_count(0)
    , works_before_reg(false) {
}

Command::~Command() {
    ServerInstance->Parser.RemoveCommand(this);
}

void Command::RegisterService() {
    if (!ServerInstance->Parser.AddCommand(this)) {
        throw ModuleException("Command already exists: " + name);
    }
}

void Command::TellNotEnoughParameters(LocalUser* user,
                                      const Params& parameters) {
    user->WriteNumeric(ERR_NEEDMOREPARAMS, name, "Not enough parameters.");
    if (ServerInstance->Config->SyntaxHints && user->registered == REG_ALL
            && syntax.length()) {
        user->WriteNumeric(RPL_SYNTAX, name, syntax);
    }
}

void Command::TellNotRegistered(LocalUser* user, const Params& parameters) {
    user->WriteNumeric(ERR_NOTREGISTERED, name, "You have not registered.");
}

SplitCommand::SplitCommand(Module* me, const std::string& cmd,
                           unsigned int minpara, unsigned int maxpara)
    : Command(me, cmd, minpara, maxpara) {
}

CmdResult SplitCommand::Handle(User* user, const Params& parameters) {
    switch (user->usertype) {
    case USERTYPE_LOCAL:
        return HandleLocal(static_cast<LocalUser*>(user), parameters);

    case USERTYPE_REMOTE:
        return HandleRemote(static_cast<RemoteUser*>(user), parameters);

    case USERTYPE_SERVER:
        return HandleServer(static_cast<FakeUser*>(user), parameters);
    }

    ServerInstance->Logs->Log("COMMAND", LOG_DEFAULT,
                              "Unknown user type %d in command (uuid=%s)!",
                              user->usertype, user->uuid.c_str());
    return CMD_INVALID;
}

CmdResult SplitCommand::HandleLocal(LocalUser* user, const Params& parameters) {
    return CMD_INVALID;
}

CmdResult SplitCommand::HandleRemote(RemoteUser* user,
                                     const Params& parameters) {
    return CMD_INVALID;
}

CmdResult SplitCommand::HandleServer(FakeUser* user, const Params& parameters) {
    return CMD_INVALID;
}
