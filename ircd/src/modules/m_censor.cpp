/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2018, 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004, 2006 Craig Edwards <brain@inspircd.org>
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

typedef insp::flat_map<std::string, std::string, irc::insensitive_swo>
CensorMap;

class ModuleCensor : public Module {
    CheckExemption::EventProvider exemptionprov;
    CensorMap censors;
    SimpleUserModeHandler cu;
    SimpleChannelModeHandler cc;

  public:
    ModuleCensor()
        : exemptionprov(this)
        , cu(this, "u_censor", 'G')
        , cc(this, "censor", 'G') {
    }

    // format of a config entry is <badword text="shit" replace="poo">
    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        if (!IS_LOCAL(user)) {
            return MOD_RES_PASSTHRU;
        }

        switch (target.type) {
        case MessageTarget::TYPE_USER: {
            User* targetuser = target.Get<User>();
            if (!targetuser->IsModeSet(cu)) {
                return MOD_RES_PASSTHRU;
            }
            break;
        }

        case MessageTarget::TYPE_CHANNEL: {
            Channel* targetchan = target.Get<Channel>();
            if (!targetchan->IsModeSet(cc)) {
                return MOD_RES_PASSTHRU;
            }

            ModResult result = CheckExemption::Call(exemptionprov, user, targetchan,
                                                    "censor");
            if (result == MOD_RES_ALLOW) {
                return MOD_RES_PASSTHRU;
            }
            break;
        }

        default:
            return MOD_RES_PASSTHRU;
        }

        for (CensorMap::const_iterator index = censors.begin(); index != censors.end(); ++index) {
            size_t censorpos;
            while ((censorpos = irc::find(details.text,
                                          index->first)) != std::string::npos) {
                if (index->second.empty()) {
                    const std::string msg =
                        InspIRCd::Format("Your message to this channel contained a banned phrase (%s) and was blocked.",
                                         index->first.c_str());
                    if (target.type == MessageTarget::TYPE_CHANNEL) {
                        user->WriteNumeric(Numerics::CannotSendTo(target.Get<Channel>(), msg));
                    } else {
                        user->WriteNumeric(Numerics::CannotSendTo(target.Get<User>(), msg));
                    }
                    return MOD_RES_DENY;
                }

                details.text.replace(censorpos, index->first.size(), index->second);
            }
        }
        return MOD_RES_PASSTHRU;
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        /*
         * reload our config file on rehash - we must destroy and re-allocate the classes
         * to call the constructor again and re-read our data.
         */
        CensorMap newcensors;
        ConfigTagList badwords = ServerInstance->Config->ConfTags("badword");
        for (ConfigIter i = badwords.first; i != badwords.second; ++i) {
            ConfigTag* tag = i->second;
            const std::string text = tag->getString("text");
            if (text.empty()) {
                throw ModuleException("<badword:text> is empty! at " + tag->getTagLocation());
            }

            const std::string replace = tag->getString("replace");
            newcensors[text] = replace;
        }
        censors.swap(newcensors);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows the server administrator to define inappropriate phrases that are not allowed to be used in private or channel messages.", VF_VENDOR);
    }

};

MODULE_INIT(ModuleCensor)
