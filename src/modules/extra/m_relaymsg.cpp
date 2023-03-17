/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 James Lu <james@overdrivenetworks.com>
 *
 * This file is a module for InspIRCd.  InspIRCd is free software: you can
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

/// $ModAuthor: jlu5
/// $ModAuthorMail: james@overdrivenetworks.com
/// $ModDepends: core 3
/// $ModDesc: Provides the RELAYMSG command & draft/relaymsg capability for stateless bridging
/// $ModConfig: <relaymsg separators="/" ident="relay" host="relay.example.com">
//  The "host" option defaults to the local server hostname if not set.

#include "inspircd.h"
#include "modules/cap.h"
#include "modules/ircv3.h"

enum {
    ERR_BADRELAYNICK = 573  // from ERR_CANNOTSENDRP in Oragono
};

// Registers the draft/relaymsg
class RelayMsgCap : public Cap::Capability {
  public:
    std::string nick_separators;

    const std::string* GetValue(LocalUser* user) const CXX11_OVERRIDE {
        return &nick_separators;
    }

    RelayMsgCap(Module* mod)
        : Cap::Capability(mod, "draft/relaymsg") {
    }
};

// Handler for the @relaymsg message tag sent with forwarded PRIVMSGs
class RelayMsgCapTag : public ClientProtocol::MessageTagProvider {
  private:
    RelayMsgCap& cap;

  public:
    RelayMsgCapTag(Module* mod, RelayMsgCap& Cap)
        : ClientProtocol::MessageTagProvider(mod)
        , cap(Cap) {
    }

    bool ShouldSendTag(LocalUser* user,
                       const ClientProtocol::MessageTagData& tagdata) CXX11_OVERRIDE {
        return cap.get(user);
    }
};

// Handler for the RELAYMSG command (users and servers)
class CommandRelayMsg : public Command {
  private:
    RelayMsgCap& cap;
    RelayMsgCapTag& captag;

  public:
    std::string fake_host;
    std::string fake_ident;

    CommandRelayMsg(Module* parent, RelayMsgCap& Cap, RelayMsgCapTag& Captag)
        : Command(parent, "RELAYMSG", 3, 3)
        , cap(Cap)
        , captag(Captag) {
        flags_needed = 'o';
        syntax = "<channel> <nick> <text>";
        allow_empty_last_param = false;
    }

    std::string GetFakeHostmask(const std::string& nick) {
        return InspIRCd::Format("%s!%s@%s", nick.c_str(), fake_ident.c_str(),
                                fake_host.c_str());
    }

    CmdResult Handle(User* user, const CommandBase::Params& parameters) {
        const std::string& channame = parameters[0];
        const std::string& nick = parameters[1];
        const std::string& text = parameters[2];

        // Check that the source has the relaymsg capability.
        if (IS_LOCAL(user) && !cap.get(user)) {
            user->WriteNumeric(ERR_NOPRIVILEGES,
                               "You must support the draft/relaymsg capability to use this command.");
            return CMD_FAILURE;
        }

        Channel* channel = ServerInstance->FindChan(channame);
        // Make sure the channel exists and the sender is in the channel
        if (!channel) {
            user->WriteNumeric(Numerics::NoSuchChannel(channame));
            return CMD_FAILURE;
        }
        if (!channel->HasUser(user)) {
            user->WriteNumeric(Numerics::CannotSendTo(channel,
                               "You must be in the channel to use this command."));
            return CMD_FAILURE;
        }

        // Check that target nick is not already in use
        if (ServerInstance->FindNick(nick)) {
            user->WriteNumeric(ERR_BADRELAYNICK, nick,
                               "RELAYMSG spoofed nick is already in use");
            return CMD_FAILURE;
        }

        // Make sure the nick doesn't include any core IRC characters (e.g. *, !)
        // This should still be more flexible than regular nick checking - in particular
        // we want to allow "/" and "~" for relayers
        if (nick.find_first_of("!+%@&#$:'\"?*,.") != std::string::npos) {
            user->WriteNumeric(ERR_BADRELAYNICK, nick,
                               "Invalid characters in spoofed nick");
            return CMD_FAILURE;
        }

        // If the sender was a lcoal user, check that the target includes a nick separator
        if (IS_LOCAL(user)) {
            if (nick.find_first_of(cap.nick_separators) == std::string::npos) {
                user->WriteNumeric(ERR_BADRELAYNICK, nick,
                                   InspIRCd::Format("Spoofed nickname must include one of the following separators: %s",
                                                    cap.nick_separators.c_str()));
                return CMD_FAILURE;
            }
        }

        // Send the message to everyone in the channel
        std::string fakeSource = GetFakeHostmask(nick);
        ClientProtocol::Messages::Privmsg privmsg(fakeSource, channel, text);
        // Tag the message as @draft/relaymsg=<nick> so the sender can recognize it
        // Also copy over tags set on the original /relaymsg command
        privmsg.AddTags(parameters.GetTags());
        privmsg.AddTag("draft/relaymsg", &captag, user->nick);
        channel->Write(ServerInstance->GetRFCEvents().privmsg, privmsg);

        if (IS_LOCAL(user)) {
            // Pass the message on to other servers
            CommandBase::Params params;
            // Preserve other message tags sent with the /relaymsg command
            params.GetTags().insert(parameters.GetTags().begin(),
                                    parameters.GetTags().end());
            params.push_back(channame);
            params.push_back(nick);
            params.push_back(":" + text);

            ServerInstance->PI->BroadcastEncap("RELAYMSG", params, user);
        }

        return CMD_SUCCESS;
    };
};

class ModuleRelayMsg : public Module {
    RelayMsgCap cap;
    RelayMsgCapTag captag;
    CommandRelayMsg cmd;

  public:
    ModuleRelayMsg() :
        cap(this),
        captag(this, cap),
        cmd(this, cap, captag) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("relaymsg");
        std::string fake_ident = tag->getString("ident", "relay");
        std::string fake_host = tag->getString("host", ServerInstance->Config->ServerName);

        // Check these before replacement so that invalid values loaded during /rehash don't get saved
        if (!ServerInstance->IsIdent(fake_ident)) {
            throw ModuleException("Invalid ident value for <relaymsg>");
        }
        if (!ServerInstance->IsHost(fake_host)) {
            throw ModuleException("Invalid host value for <relaymsg>");
        }
        cmd.fake_host = fake_host;
        cmd.fake_ident = fake_ident;
        cap.nick_separators = tag->getString("separators", "/", 1);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the RELAYMSG command for stateless bridging");
    }
};

MODULE_INIT(ModuleRelayMsg)
