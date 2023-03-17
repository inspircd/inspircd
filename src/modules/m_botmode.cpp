/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Shawn Smith <ShawnSmith0828@gmail.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "modules/ctctags.h"
#include "modules/who.h"
#include "modules/whois.h"

class BotTag : public ClientProtocol::MessageTagProvider {
  private:
    SimpleUserModeHandler& botmode;
    CTCTags::CapReference ctctagcap;

  public:
    BotTag(Module* mod, SimpleUserModeHandler& bm)
        : ClientProtocol::MessageTagProvider(mod)
        , botmode(bm)
        , ctctagcap(mod) {
    }

    void OnPopulateTags(ClientProtocol::Message& msg) CXX11_OVERRIDE {
        User* const user = msg.GetSourceUser();
        if (!user || !user->IsModeSet(botmode)) {
            return;
        }

        // TODO: remove inspircd.org/bot in v4.
        msg.AddTag("bot", this, "");
        msg.AddTag("inspircd.org/bot", this, "");
    }

    bool ShouldSendTag(LocalUser* user,
                       const ClientProtocol::MessageTagData& tagdata) CXX11_OVERRIDE {
        return ctctagcap.get(user);
    }
};

class ModuleBotMode
    : public Module
    , public Who::EventListener
    , public Whois::EventListener {
  private:
    SimpleUserModeHandler bm;
    BotTag tag;
    bool forcenotice;

  public:
    ModuleBotMode()
        : Who::EventListener(this)
        , Whois::EventListener(this)
        , bm(this, "bot", 'B')
        , tag(this, bm) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        forcenotice = ServerInstance->Config->ConfValue("botmode")->getBool("forcenotice");
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["BOT"] = ConvToStr(bm.GetModeChar());
    }

    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        // Allow sending if forcenotice is off, the user is not a bot, or if the message is a notice.
        if (!forcenotice || !user->IsModeSet(bm) || details.type == MSG_NOTICE) {
            return MOD_RES_PASSTHRU;
        }

        // Allow sending PRIVMSGs to services pseudoclients.
        if (target.type == MessageTarget::TYPE_USER && target.Get<User>()->server->IsULine()) {
            return MOD_RES_PASSTHRU;
        }

        // Force the message to be broadcast as a NOTICE.
        details.type = MSG_NOTICE;
        return MOD_RES_PASSTHRU;
    }

    ModResult OnWhoLine(const Who::Request& request, LocalUser* source, User* user,
                        Membership* memb, Numeric::Numeric& numeric) CXX11_OVERRIDE {
        size_t flag_index;
        if (!request.GetFieldIndex('f', flag_index)) {
            return MOD_RES_PASSTHRU;
        }

        if (user->IsModeSet(bm)) {
            numeric.GetParams()[flag_index].push_back('B');
        }

        return MOD_RES_PASSTHRU;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds user mode B (bot) which marks users with it set as bots in their /WHOIS response.",VF_VENDOR);
    }

    void OnWhois(Whois::Context& whois) CXX11_OVERRIDE {
        if (whois.GetTarget()->IsModeSet(bm)) {
            whois.SendLine(RPL_WHOISBOT, "is a bot on " + ServerInstance->Config->Network);
        }
    }
};

MODULE_INIT(ModuleBotMode)
