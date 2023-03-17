/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2017-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014, 2016 Attila Molnar <attilamolnar@hush.com>
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
#include "xline.h"
#include "core_xline.h"

bool InsaneBan::MatchesEveryone(const std::string& mask, MatcherBase& test,
                                User* user, char bantype, const char* confkey) {
    ConfigTag* insane = ServerInstance->Config->ConfValue("insane");

    if (insane->getBool(confkey)) {
        return false;
    }

    float itrigger = insane->getFloat("trigger", 95.5, 0.0, 100.0);

    long matches = test.Run(mask);

    if (!matches) {
        return false;
    }

    float percent = ((float)matches / (float)
                     ServerInstance->Users->GetUsers().size()) * 100;
    if (percent > itrigger) {
        const char* article = strchr("AEIOUaeiou", bantype) ? "an" : "a";
        ServerInstance->SNO->WriteToSnoMask('x',
                                            "\002WARNING\002: %s tried to set add %s %c-line on %s which covers %.2f%% of the network which is more than the maximum of %.2f%%!",
                                            user->nick.c_str(), article, bantype, mask.c_str(), percent, itrigger);
        user->WriteNotice(
            InspIRCd::Format("*** Unable to add %s %c-line on %s which covers %.2f%% of the network which is more than the maximum of %.2f%%!",
                             article, bantype, mask.c_str(), percent, itrigger));
        return true;
    }
    return false;
}

bool InsaneBan::IPHostMatcher::Check(User* user,
                                     const std::string& mask) const {
    return ((InspIRCd::MatchCIDR(user->MakeHost(), mask,
                                 ascii_case_insensitive_map)) ||
            (InspIRCd::MatchCIDR(user->MakeHostIP(), mask, ascii_case_insensitive_map)));
}

class CoreModXLine : public Module {
    CommandEline cmdeline;
    CommandGline cmdgline;
    CommandKline cmdkline;
    CommandQline cmdqline;
    CommandZline cmdzline;

  public:
    CoreModXLine()
        : cmdeline(this), cmdgline(this), cmdkline(this), cmdqline(this),
          cmdzline(this) {
    }

    void OnSetUserIP(LocalUser* user) CXX11_OVERRIDE {
        if (user->quitting) {
            return;
        }

        user->exempt = (ServerInstance->XLines->MatchesLine("E", user) != NULL);
        user->CheckLines(true);
    }

    void OnPostChangeRealHost(User* user) CXX11_OVERRIDE {
        LocalUser* luser = IS_LOCAL(user);
        if (!luser || luser->quitting) {
            return;
        }

        luser->exempt = (ServerInstance->XLines->MatchesLine("E", user) != NULL);
        luser->CheckLines(false);
    }

    ModResult OnUserPreNick(LocalUser* user,
                            const std::string& newnick) CXX11_OVERRIDE {
        // Check Q-lines (for local nick changes only, remote servers have our Q-lines to enforce themselves)

        XLine* xline = ServerInstance->XLines->MatchesLine("Q", newnick);
        if (!xline) {
            return MOD_RES_PASSTHRU;    // No match
        }

        // A Q-line matched the new nick, tell opers if the user is registered
        if (user->registered == REG_ALL) {
            ServerInstance->SNO->WriteGlobalSno('x', "Q-lined nickname %s from %s: %s",
                                                newnick.c_str(), user->GetFullRealHost().c_str(), xline->reason.c_str());
        }

        // Send a numeric because if we deny then the core doesn't reply anything
        user->WriteNumeric(ERR_ERRONEUSNICKNAME, newnick, InspIRCd::Format("Invalid nickname: %s", xline->reason.c_str()));
        return MOD_RES_DENY;
    }

    void OnGarbageCollect() CXX11_OVERRIDE {
        // HACK: ELines are not expired properly at the moment but it can't be fixed
        // as the XLine system is a spaghetti nightmare. Instead we skip over expired
        // ELines in XLineManager::CheckELines() and expire them here instead.
        ServerInstance->XLines->GetAll("E");
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the ELINE, GLINE, KLINE, QLINE, and ZLINE commands", VF_VENDOR|VF_CORE);
    }
};

MODULE_INIT(CoreModXLine)
