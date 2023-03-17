/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon@snoonet.org>
 *   Copyright (C) 2016 Foxlet <foxlet@furcode.co>
 *   Copyright (C) 2015 Adam <Adam@anope.org>
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

/// $ModAuthor: Adam
/// $ModAuthorMail: adam@anope.org
/// $ModConfig: <slowmode modechar="W">
/// $ModDesc: Provides channel mode +W (slow mode)
/// $ModDepends: core 3

/** Holds slowmode settings and state for mode +W
 */
class slowmodesettings {
  public:
    typedef std::map<User*, unsigned int> user_counter_t;

    unsigned int lines;
    unsigned int secs;

    bool user;

    union {
        unsigned int counter;
        user_counter_t* user_counter;
    };

    time_t reset;

    slowmodesettings(int l, int s, bool u = false) : lines(l), secs(s), user(u),
        user_counter(0) {
        this->clear();
        reset = ServerInstance->Time() + secs;
    }

    bool addmessage(User *who) {
        if (ServerInstance->Time() > reset) {
            this->clear();
            reset = ServerInstance->Time() + secs;
        }

        if (user)
            if (IS_LOCAL(who)) {
                return ++((*user_counter)[who]) >= lines;
            } else {
                return false;
            } else {
            return ++counter >= lines;
        }
    }

    void clear() {
        if (user)
            if (user_counter) {
                user_counter->clear();
            } else {
                user_counter = new user_counter_t;
            } else {
            counter = 0;
        }
    }
};

/** Handles channel mode +W
 */
class MsgFlood : public ParamMode<MsgFlood, SimpleExtItem<slowmodesettings> > {
  public:
    MsgFlood(Module* Creator)
        : ParamMode<MsgFlood, SimpleExtItem<slowmodesettings> >(Creator, "slowmode",
                ServerInstance->Config->ConfValue("slowmode")->getString("modechar", "W", 1,
                        1)[0]) {
#if defined INSPIRCD_VERSION_SINCE && INSPIRCD_VERSION_SINCE(3, 2)
        syntax = "[cu]<lines>:<seconds>";
#endif
    }

    ModeAction OnSet(User* source, Channel* channel,
                     std::string& parameter) CXX11_OVERRIDE {
        std::string::size_type colon = parameter.find(':');
        if (colon == std::string::npos || parameter.find('-') != std::string::npos) {
            source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
            return MODEACTION_DENY;
        }

        bool user;
        switch (parameter[0]) {
        case 'u':
            user = true;
            parameter.erase(0, 1);
            colon--;
            break;
        case 'c':
            parameter.erase(0, 1);
            colon--;
        /*@fallthrough@*/
        default:
            user = false;
            break;
        }

        /* Set up the slowmode parameters for this channel */
        unsigned int nlines = ConvToNum<unsigned int>(parameter.substr(0, colon));
        unsigned int nsecs = ConvToNum<unsigned int>(parameter.substr(colon + 1));

        if ((nlines < 2) || nsecs < 1) {
            source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
            return MODEACTION_DENY;
        }

        slowmodesettings* f = ext.get(channel);
        if (f && nlines == f->lines && nsecs == f->secs && user == f->user)
            // mode params match
        {
            return MODEACTION_DENY;
        }

        ext.set(channel, new slowmodesettings(nlines, nsecs, user));
        parameter = std::string(user ? "u" : "c") + ConvToStr(nlines) + ":" + ConvToStr(nsecs);
        return MODEACTION_ALLOW;
    }

    void SerializeParam(Channel* chan, const slowmodesettings* sms,
                        std::string& out) {
        out.push_back(sms->user ? 'u' : 'c');
        out.append(ConvToStr(sms->lines));
        out.push_back(':');
        out.append(ConvToStr(sms->secs));
    }
};

class ModuleMsgFlood : public Module {
    MsgFlood mf;
    CheckExemption::EventProvider exemptionprov;

  public:
    ModuleMsgFlood()
        : mf(this)
        , exemptionprov(this) {
    }

    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        if (target.type != MessageTarget::TYPE_CHANNEL || user->server->IsULine()) {
            return MOD_RES_PASSTHRU;
        }

        Channel* dest = target.Get<Channel>();
        if (!dest->IsModeSet(mf)) {
            return MOD_RES_PASSTHRU;
        }

        if (CheckExemption::Call(exemptionprov, user, dest, "slowmode") == MOD_RES_ALLOW) {
            return MOD_RES_PASSTHRU;
        }

        slowmodesettings *f = mf.ext.get(dest);
        if (f == NULL || !f->addmessage(user)) {
            return MOD_RES_PASSTHRU;
        }

        if (!IS_LOCAL(user)) {
            return MOD_RES_PASSTHRU;
        }

        user->WriteNumeric(ERR_CANNOTSENDTOCHAN, dest->name, "Message throttled due to flood");
        return MOD_RES_DENY;
    }

    Version GetVersion() CXX11_OVERRIDE {
        std::string valid_param("[u|c]<lines>:<secs>");
        return Version("Provides channel mode +" + ConvToStr(mf.GetModeChar()) + " (slowmode)", VF_COMMON, valid_param);
    }
};

MODULE_INIT(ModuleMsgFlood)
