/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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

/// $ModAuthor: Attila Molnar
/// $ModAuthorMail: attilamolnar@hush.com
/// $ModConfig: <extbanredirect char="d">
/// $ModDepends: core 3
/// $ModDesc: Provide extended ban <extbanchar>:<chan>:<mask> to redirect users to another channel


#include "inspircd.h"
#include "listmode.h"

enum {
    // From UnrealIRCd
    ERR_LINKCHANNEL = 470,
    // From m_banredirect
    ERR_REDIRECT = 690
};

class BanWatcher : public ModeWatcher {
  public:
    char extbanchar;

    BanWatcher(Module* parent)
        : ModeWatcher(parent, "ban", MODETYPE_CHANNEL) {
    }

    bool IsExtBanRedirect(const std::string& mask) {
        // d:#targetchan:*!*@Attila.inspircd.org
        return (mask.length() > 2 && mask[0] == extbanchar && mask[1] == ':');
    }

    bool BeforeMode(User* source, User*, Channel* channel, std::string& param,
                    bool adding) CXX11_OVERRIDE {
        if (!IS_LOCAL(source) || !channel || !adding) {
            return true;
        }

        if (!IsExtBanRedirect(param)) {
            return true;
        }

        std::string::size_type p = param.find(':', 2);
        if (p == std::string::npos) {
            source->WriteNumeric(ERR_REDIRECT,
                                 InspIRCd::Format("Extban redirect \"%s\" is invalid. Format: %c:<chan>:<mask>",
                                                  param.c_str(), extbanchar));
            return false;
        }

        std::string targetname(param, 2, p - 2);
        if (!ServerInstance->IsChannel(targetname)) {
            source->WriteNumeric(ERR_NOSUCHCHANNEL, channel->name,
                                 InspIRCd::Format("Invalid channel name in redirection (%s)",
                                                  targetname.c_str()));
            return false;
        }

        Channel* const targetchan = ServerInstance->FindChan(targetname);
        if (!targetchan) {
            source->WriteNumeric(ERR_NOSUCHCHANNEL, channel->name,
                                 InspIRCd::Format("Target channel %s must exist to be set as a redirect.",
                                                  targetname.c_str()));
            return false;
        }

        if (targetchan == channel) {
            source->WriteNumeric(ERR_NOSUCHCHANNEL, channel->name,
                                 "You cannot set a ban redirection to the channel the ban is on");
            return false;
        }

        if (targetchan->GetPrefixValue(source) < OP_VALUE) {
            source->WriteNumeric(ERR_CHANOPRIVSNEEDED, channel->name,
                                 InspIRCd::Format("You must be opped on %s to set it as a redirect.",
                                                  targetname.c_str()));
            return false;
        }

        return true;
    }
};

class ModuleExtBanRedirect : public Module {
    ChanModeReference limitmode;
    ChanModeReference limitredirect;
    BanWatcher banwatcher;
    bool active;

  public:
    ModuleExtBanRedirect()
        : limitmode(this, "limit")
        , limitredirect(this, "redirect")
        , banwatcher(this)
        , active(false) {
    }

    void ReadConfig(ConfigStatus&) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("extbanredirect");
        banwatcher.extbanchar = tag->getString("char", "d", 1, 1)[0];
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["EXTBAN"].push_back(banwatcher.extbanchar);
    }

    ModResult OnCheckBan(User* user, Channel* chan,
                         const std::string& mask) CXX11_OVERRIDE {
        LocalUser* localuser = IS_LOCAL(user);

        if (active || !localuser) {
            return MOD_RES_PASSTHRU;
        }

        if (!banwatcher.IsExtBanRedirect(mask)) {
            return MOD_RES_PASSTHRU;
        }

        std::string::size_type p = mask.find(':', 2);
        if (p == std::string::npos) {
            return MOD_RES_PASSTHRU;
        }

        if (!chan->CheckBan(localuser, mask.substr(p + 1))) {
            return MOD_RES_PASSTHRU;
        }

        const std::string targetname = mask.substr(2, p - 2);
        Channel* const target = ServerInstance->FindChan(targetname);
        if (target && target->IsModeSet(limitmode)) {
            if (target->IsModeSet(limitredirect)
                    && target->GetUserCounter() >= ConvToNum<size_t>(target->GetModeParameter(
                                limitmode))) {
                // The core will send "You're banned"
                return MOD_RES_DENY;
            }
        }

        // Ok to redirect
        // The core will send "You're banned"
        localuser->WriteNumeric(ERR_LINKCHANNEL, chan->name, targetname, "You are banned from this channel, so you are automatically being transferred to the redirected channel.");
        active = true;
        Channel::JoinUser(localuser, targetname);
        active = false;

        return MOD_RES_DENY;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version(InspIRCd::Format("Provides extended ban %c:<chan>:<mask> to redirect users to another channel", banwatcher.extbanchar), VF_OPTCOMMON);
    }
};

MODULE_INIT(ModuleExtBanRedirect)
