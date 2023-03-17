/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "listmode.h"
#include "modules/exemption.h"

enum {
    RPL_ENDOFEXEMPTIONLIST = 953,
    RPL_EXEMPTIONLIST = 954
};

class ExemptChanOps : public ListModeBase {
  public:
    ExemptChanOps(Module* Creator)
        : ListModeBase(Creator, "exemptchanops", 'X',
                       "End of channel exemptchanops list", RPL_EXEMPTIONLIST, RPL_ENDOFEXEMPTIONLIST,
                       false) {
        syntax = "<restriction>:<prefix>";
    }

    static PrefixMode* FindMode(const std::string& mode) {
        if (mode.length() == 1) {
            return ServerInstance->Modes->FindPrefixMode(mode[0]);
        }

        ModeHandler* mh = ServerInstance->Modes->FindMode(mode, MODETYPE_CHANNEL);
        return mh ? mh->IsPrefixMode() : NULL;
    }

    static bool ParseEntry(const std::string& entry, std::string& restriction,
                           std::string& prefix) {
        // The entry must be in the format <restriction>:<prefix>.
        std::string::size_type colon = entry.find(':');
        if (colon == std::string::npos || colon == entry.length()-1) {
            return false;
        }

        restriction.assign(entry, 0, colon);
        prefix.assign(entry, colon + 1, std::string::npos);
        return true;
    }

    ModResult AccessCheck(User* source, Channel* channel, std::string& parameter,
                          bool adding) CXX11_OVERRIDE {
        std::string restriction;
        std::string prefix;
        if (!ParseEntry(parameter, restriction, prefix)) {
            return MOD_RES_PASSTHRU;
        }

        PrefixMode* pm = FindMode(prefix);
        if (!pm) {
            return MOD_RES_PASSTHRU;
        }

        if (channel->GetPrefixValue(source) >= pm->GetLevelRequired(adding)) {
            return MOD_RES_PASSTHRU;
        }

        source->WriteNumeric(ERR_CHANOPRIVSNEEDED, channel->name, InspIRCd::Format("You must be able to %s mode %c (%s) to %s a restriction containing it",
                             adding ? "set" : "unset", pm->GetModeChar(), pm->name.c_str(), adding ? "add" : "remove"));
        return MOD_RES_DENY;
    }

    bool ValidateParam(User* user, Channel* chan,
                       std::string& word) CXX11_OVERRIDE {
        std::string restriction;
        std::string prefix;
        if (!ParseEntry(word, restriction, prefix)) {
            user->WriteNumeric(Numerics::InvalidModeParameter(chan, this, word));
            return false;
        }

        // If there is a '-' in the restriction string ignore it and everything after it
        // to support "auditorium-vis" and "auditorium-see" in m_auditorium
        std::string::size_type dash = restriction.find('-');
        if (dash != std::string::npos) {
            restriction.erase(dash);
        }

        if (!ServerInstance->Modes->FindMode(restriction, MODETYPE_CHANNEL)) {
            user->WriteNumeric(Numerics::InvalidModeParameter(chan, this, word,
                               "Unknown restriction."));
            return false;
        }

        if (prefix != "*" && !FindMode(prefix)) {
            user->WriteNumeric(Numerics::InvalidModeParameter(chan, this, word,
                               "Unknown prefix mode."));
            return false;
        }

        return true;
    }
};

class ExemptHandler : public CheckExemption::EventListener {
  public:
    ExemptChanOps ec;
    ExemptHandler(Module* me)
        : CheckExemption::EventListener(me)
        , ec(me) {
    }

    ModResult OnCheckExemption(User* user, Channel* chan,
                               const std::string& restriction) CXX11_OVERRIDE {
        unsigned int mypfx = chan->GetPrefixValue(user);
        std::string minmode;

        ListModeBase::ModeList* list = ec.GetList(chan);

        if (list) {
            for (ListModeBase::ModeList::iterator i = list->begin(); i != list->end();
                    ++i) {
                std::string::size_type pos = (*i).mask.find(':');
                if (pos == std::string::npos) {
                    continue;
                }
                if (!i->mask.compare(0, pos, restriction)) {
                    minmode.assign(i->mask, pos + 1, std::string::npos);
                }
            }
        }

        PrefixMode* mh = ExemptChanOps::FindMode(minmode);
        if (mh && mypfx >= mh->GetPrefixRank()) {
            return MOD_RES_ALLOW;
        }
        if (mh || minmode == "*") {
            return MOD_RES_DENY;
        }

        return MOD_RES_PASSTHRU;
    }
};

class ModuleExemptChanOps : public Module {
    ExemptHandler eh;

  public:
    ModuleExemptChanOps() : eh(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds channel mode X (exemptchanops) which allows channel operators to grant exemptions to various channel-level restrictions.", VF_VENDOR);
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        eh.ec.DoRehash();
    }
};

MODULE_INIT(ModuleExemptChanOps)
