/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2017, 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Shawn Smith <ShawnSmith0828@gmail.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 DjSlash <djslash@djslash.org>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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
#include "modules/exemption.h"

class ModuleBlockColor : public Module {
    CheckExemption::EventProvider exemptionprov;
    SimpleChannelModeHandler bc;
  public:

    ModuleBlockColor()
        : exemptionprov(this)
        , bc(this, "blockcolor", 'c') {
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["EXTBAN"].push_back('c');
    }

    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        if ((target.type == MessageTarget::TYPE_CHANNEL) && (IS_LOCAL(user))) {
            Channel* c = target.Get<Channel>();

            ModResult res = CheckExemption::Call(exemptionprov, user, c, "blockcolor");
            if (res == MOD_RES_ALLOW) {
                return MOD_RES_PASSTHRU;
            }

            bool modeset = c->IsModeSet(bc);
            if (!c->GetExtBanStatus(user, 'c').check(!modeset)) {
                std::string ctcpname; // Unused.
                std::string message;
                if (!details.IsCTCP(ctcpname, message)) {
                    message.assign(details.text);
                }

                for (std::string::iterator i = message.begin(); i != message.end(); ++i) {
                    const unsigned char chr = static_cast<unsigned char>(*i);
                    if (chr < 32) {
                        if (modeset) {
                            user->WriteNumeric(Numerics::CannotSendTo(c,
                                               "messages containing formatting characters", &bc));
                        } else {
                            user->WriteNumeric(Numerics::CannotSendTo(c,
                                               "messages containing formatting characters", 'c', "nocolor"));
                        }
                        return MOD_RES_DENY;
                    }
                }
            }
        }
        return MOD_RES_PASSTHRU;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds channel mode c (blockcolor) which allows channels to block messages which contain IRC formatting codes.",VF_VENDOR);
    }
};

MODULE_INIT(ModuleBlockColor)
