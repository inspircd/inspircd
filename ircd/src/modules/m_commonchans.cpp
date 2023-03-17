/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017, 2019-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "modules/ctctags.h"

class ModuleCommonChans
    : public CTCTags::EventListener
    , public Module {
  private:
    SimpleUserModeHandler mode;
    bool invite;

    bool IsExempt(User* source, User* target) {
        if (!target->IsModeSet(mode) || source->SharesChannelWith(target)) {
            return true;    // Target doesn't have mode set or shares a common channel.
        }

        if (source->HasPrivPermission("users/ignore-commonchans")
                || source->server->IsULine()) {
            return true;    // Source is an oper or a uline.
        }

        return false;
    }

    ModResult HandleMessage(User* user, const MessageTarget& target) {
        if (target.type != MessageTarget::TYPE_USER) {
            return MOD_RES_PASSTHRU;
        }

        User* targetuser = target.Get<User>();
        if (IsExempt(user, targetuser)) {
            return MOD_RES_PASSTHRU;
        }

        user->WriteNumeric(Numerics::CannotSendTo(targetuser, "messages", &mode));
        return MOD_RES_DENY;
    }

  public:
    ModuleCommonChans()
        : CTCTags::EventListener(this)
        , mode(this, "deaf_commonchan", 'c') {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds user mode c (deaf_commonchan) which requires users to have a common channel before they can privately message each other.", VF_VENDOR);
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("commonchans");
        invite = tag->getBool("invite");
    }

    ModResult OnUserPreInvite(User* source, User* dest, Channel* channel,
                              time_t timeout) CXX11_OVERRIDE {
        if (!invite || IsExempt(source, dest)) {
            return MOD_RES_PASSTHRU;
        }

        source->WriteNumeric(Numerics::CannotSendTo(dest, "invites", &mode));
        return MOD_RES_DENY;
    }

    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        return HandleMessage(user, target);
    }

    ModResult OnUserPreTagMessage(User* user, const MessageTarget& target,
                                  CTCTags::TagMessageDetails& details) CXX11_OVERRIDE {
        return HandleMessage(user, target);
    }
};

MODULE_INIT(ModuleCommonChans)
