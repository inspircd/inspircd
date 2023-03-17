/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2017, 2021-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Dennis Friis <peavey@inspircd.org>
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

// User mode +d - filter out channel messages and channel notices
class DeafMode : public ModeHandler {
  public:
    DeafMode(Module* Creator) : ModeHandler(Creator, "deaf", 'd', PARAM_NONE,
                                                MODETYPE_USER) { }

    ModeAction OnModeChange(User* source, User* dest, Channel* channel,
                            std::string& parameter, bool adding) CXX11_OVERRIDE {
        if (adding == dest->IsModeSet(this)) {
            return MODEACTION_DENY;
        }

        if (adding) {
            dest->WriteNotice("*** You have enabled user mode +d, deaf mode. This mode means you WILL NOT receive any messages from any channels you are in. If you did NOT mean to do this, use /mode "
                              + dest->nick + " -d.");
        }

        dest->SetMode(this, adding);
        return MODEACTION_ALLOW;
    }
};

// User mode +D - filter out user messages and user notices
class PrivDeafMode : public ModeHandler {
  public:
    PrivDeafMode(Module* Creator) : ModeHandler(Creator, "privdeaf", 'D',
                PARAM_NONE, MODETYPE_USER) {
        if (!ServerInstance->Config->ConfValue("deaf")->getBool("enableprivdeaf")) {
            DisableAutoRegister();
        }
    }

    ModeAction OnModeChange(User* source, User* dest, Channel* channel,
                            std::string& parameter, bool adding) CXX11_OVERRIDE {
        if (adding == dest->IsModeSet(this)) {
            return MODEACTION_DENY;
        }

        if (adding) {
            dest->WriteNotice("*** You have enabled user mode +D, private deaf mode. This mode means you WILL NOT receive any messages and notices from any nicks. If you did NOT mean to do this, use /mode "
                              + dest->nick + " -D.");
        }

        dest->SetMode(this, adding);
        return MODEACTION_ALLOW;
    }
};

class ModuleDeaf
    : public Module
    , public CTCTags::EventListener {
  private:
    DeafMode deafmode;
    PrivDeafMode privdeafmode;
    std::string deaf_bypasschars;
    std::string deaf_bypasschars_uline;
    bool privdeafuline;

    ModResult HandleChannel(User* source, Channel* target, CUList& exemptions,
                            bool is_bypasschar, bool is_bypasschar_uline) {
        const Channel::MemberMap& ulist = target->GetUsers();
        for (Channel::MemberMap::const_iterator i = ulist.begin(); i != ulist.end();
                ++i) {
            User* member = i->first;

            // Allow if the user doesn't have the mode set.
            if (!member->IsModeSet(deafmode)) {
                continue;
            }

            // Allow if the message begins with a uline char and the
            // user is on a ulined server.
            if (is_bypasschar_uline && member->server->IsULine()) {
                continue;
            }

            // Allow if the prefix begins with a normal char and the
            // user is not on a ulined server.
            if (is_bypasschar && !member->server->IsULine()) {
                continue;
            }

            exemptions.insert(member);
        }

        return MOD_RES_PASSTHRU;
    }

    ModResult HandleUser(User* source, User* target) {
        // Allow if the mode is not set.
        if (!target->IsModeSet(privdeafmode)) {
            return MOD_RES_PASSTHRU;
        }

        // Reject if the source is ulined and privdeafuline is disabled.
        if (!privdeafuline && source->server->IsULine()) {
            return MOD_RES_DENY;
        }

        // Reject if the source doesn't have the right priv.
        if (!source->HasPrivPermission("users/ignore-privdeaf")) {
            return MOD_RES_DENY;
        }

        return MOD_RES_PASSTHRU;
    }

  public:
    ModuleDeaf()
        : CTCTags::EventListener(this)
        , deafmode(this)
        , privdeafmode(this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("deaf");
        deaf_bypasschars = tag->getString("bypasschars");
        deaf_bypasschars_uline = tag->getString("bypasscharsuline");
        privdeafuline = tag->getBool("privdeafuline", true);
    }

    ModResult OnUserPreTagMessage(User* user, const MessageTarget& target,
                                  CTCTags::TagMessageDetails& details) CXX11_OVERRIDE {
        switch (target.type) {
        case MessageTarget::TYPE_CHANNEL:
            return HandleChannel(user, target.Get<Channel>(), details.exemptions, false,
                                 false);

        case MessageTarget::TYPE_USER:
            return HandleUser(user, target.Get<User>());

        case MessageTarget::TYPE_SERVER:
            break;
        }

        return MOD_RES_PASSTHRU;
    }

    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        switch (target.type) {
        case MessageTarget::TYPE_CHANNEL: {
            // If we have no bypasschars_uline in config, and this is a bypasschar (regular)
            // Then it is obviously going to get through +d, no exemption list required
            bool is_bypasschar = (deaf_bypasschars.find(details.text[0]) !=
                                  std::string::npos);
            if (deaf_bypasschars_uline.empty() && is_bypasschar) {
                return MOD_RES_PASSTHRU;
            }

            // If it matches both bypasschar and bypasschar_uline, it will get through.
            bool is_bypasschar_uline = (deaf_bypasschars_uline.find(
                                            details.text[0]) != std::string::npos);
            if (is_bypasschar && is_bypasschar_uline) {
                return MOD_RES_PASSTHRU;
            }

            return HandleChannel(user, target.Get<Channel>(), details.exemptions,
                                 is_bypasschar, is_bypasschar_uline);
        }

        case MessageTarget::TYPE_USER:
            return HandleUser(user, target.Get<User>());

        case MessageTarget::TYPE_SERVER:
            break;
        }

        return MOD_RES_PASSTHRU;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds user modes d (deaf) and D (privdeaf) which prevents users from receiving channel (deaf) or private (privdeaf) messages.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleDeaf)
