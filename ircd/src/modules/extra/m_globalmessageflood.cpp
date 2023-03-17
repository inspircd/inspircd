/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon@snoonet.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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

/// $ModConfig: <globalflood modechar="x">
/// $ModDesc: Provides channel mode +x (oper only top-level channel flood protection with SNOMASK +F)
/// $ModDepends: core 3

typedef std::map<User*, unsigned int> counter_t;

/** Holds flood settings and state for mode +x
 */
class globalfloodsettings {
  public:
    bool ban;
    unsigned int secs;
    unsigned int lines;
    time_t reset;
    counter_t counters;

    globalfloodsettings(bool a, int b, int c) : ban(a), secs(b), lines(c) {
        reset = ServerInstance->Time() + secs;
    }

    bool addmessage(User* who) {
        if (ServerInstance->Time() > reset) {
            counters.clear();
            reset = ServerInstance->Time() + secs;
        }

        return (++counters[who] >= this->lines);
    }

    void clear(User* who) {
        counter_t::iterator iter = counters.find(who);
        if (iter != counters.end()) {
            counters.erase(iter);
        }
    }
};

/** Handles channel mode +x
 */
class GlobalMsgFlood : public
    ParamMode<GlobalMsgFlood, SimpleExtItem<globalfloodsettings> > {
  public:
    /* This an oper only mode */
    GlobalMsgFlood(Module* Creator)
        : ParamMode<GlobalMsgFlood, SimpleExtItem<globalfloodsettings> >(Creator,
                "globalflood",
                ServerInstance->Config->ConfValue("globalflood")->getString("modechar", "x", 1,
                        1)[0]) {
#if defined INSPIRCD_VERSION_SINCE && INSPIRCD_VERSION_SINCE(3, 2)
        syntax = "[*]<lines>:<seconds>";
#endif
        oper = true;
    }

    ModeAction OnSet(User* source, Channel* channel,
                     std::string& parameter) CXX11_OVERRIDE {
        std::string::size_type colon = parameter.find(':');
        if ((colon == std::string::npos) || (parameter.find('-') != std::string::npos)) {
            source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
            return MODEACTION_DENY;
        }

        /* Set up the flood parameters for this channel */
        bool ban = (parameter[0] == '*');
        unsigned int nlines = ConvToNum<unsigned int>(parameter.substr(ban ? 1 : 0, ban ? colon-1 : colon));
        unsigned int nsecs = ConvToNum<unsigned int>(parameter.substr(colon + 1));

        if ((nlines<2) || (nsecs<1)) {
            source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
            return MODEACTION_DENY;
        }

        globalfloodsettings* f = ext.get(channel);
        if ((f) && (nlines == f->lines) && (nsecs == f->secs) && (ban == f->ban))
            // mode params match
        {
            return MODEACTION_DENY;
        }

        ext.set(channel, new globalfloodsettings(ban, nsecs, nlines));
        return MODEACTION_ALLOW;
    }

    void SerializeParam(Channel* chan, const globalfloodsettings* gfs,
                        std::string& out) {
        out.append((gfs->ban ? "*" : "") + ConvToStr(gfs->lines) + ":" + ConvToStr(
                       gfs->secs));
    }
};

class ModuleGlobalMsgFlood : public Module {
    GlobalMsgFlood mf;

  public:
    ModuleGlobalMsgFlood()
        : mf(this) {
    }

    void init() CXX11_OVERRIDE {
        /* Enables Flood announcements for everyone with +s +f */
        ServerInstance->SNO->EnableSnomask('f', "FLOOD");
    }

    ModResult ProcessMessages(User* user, Channel* dest, const std::string& text) {
        if ((!IS_LOCAL(user)) || !dest->IsModeSet(mf)) {
            return MOD_RES_PASSTHRU;
        }

        if (user->IsModeSet('o')) {
            return MOD_RES_PASSTHRU;
        }

        globalfloodsettings *f = mf.ext.get(dest);
        if (f) {
            if (f->addmessage(user)) {
                f->clear(user);
                /* Generate the SNOTICE when someone triggers the flood limit */

                ServerInstance->SNO->WriteGlobalSno('f',
                                                    "Global channel flood triggered by %s (%s) in %s (limit was %u lines in %u secs)",
                                                    user->GetFullRealHost().c_str(), user->GetFullHost().c_str(),
                                                    dest->name.c_str(), f->lines, f->secs);

                return MOD_RES_DENY;
            }
        }

        return MOD_RES_PASSTHRU;
    }

    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        if (target.type == MessageTarget::TYPE_CHANNEL) {
            return ProcessMessages(user, target.Get<Channel>(), details.text);
        }

        return MOD_RES_PASSTHRU;
    }

    void Prioritize() CXX11_OVERRIDE {
        // we want to be after all modules that might deny the message (e.g. m_muteban, m_noctcp, m_blockcolor, etc.)
        ServerInstance->Modules->SetPriority(this, I_OnUserPreMessage, PRIORITY_LAST);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides channel mode +x (oper-only message flood protection)");
    }
};

MODULE_INIT(ModuleGlobalMsgFlood)
