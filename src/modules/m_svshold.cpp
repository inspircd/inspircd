/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2018, 2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
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
#include "xline.h"
#include "modules/stats.h"

namespace {
bool silent;
}

/** Holds a SVSHold item
 */
class SVSHold : public XLine {
  public:
    std::string nickname;

    SVSHold(time_t s_time, unsigned long d, const std::string& src,
            const std::string& re, const std::string& nick)
        : XLine(s_time, d, src, re, "SVSHOLD")
        , nickname(nick) {
    }

    bool Matches(User* u) CXX11_OVERRIDE {
        if (u->nick == nickname) {
            return true;
        }
        return false;
    }

    bool Matches(const std::string& s) CXX11_OVERRIDE {
        return InspIRCd::Match(s, nickname);
    }

    void DisplayExpiry() CXX11_OVERRIDE {
        if (!silent) {
            XLine::DisplayExpiry();
        }
    }

    const std::string& Displayable() CXX11_OVERRIDE {
        return nickname;
    }
};

/** An XLineFactory specialized to generate SVSHOLD pointers
 */
class SVSHoldFactory : public XLineFactory {
  public:
    SVSHoldFactory() : XLineFactory("SVSHOLD") { }

    /** Generate an SVSHOLD
    */
    XLine* Generate(time_t set_time, unsigned long duration,
                    const std::string& source, const std::string& reason,
                    const std::string& xline_specific_mask) CXX11_OVERRIDE {
        return new SVSHold(set_time, duration, source, reason, xline_specific_mask);
    }

    bool AutoApplyToUserList(XLine* x) CXX11_OVERRIDE {
        return false;
    }
};

/** Handle /SVSHold
 */
class CommandSvshold : public Command {
  public:
    CommandSvshold(Module* Creator) : Command(Creator, "SVSHOLD", 1) {
        flags_needed = 'o';
        this->syntax = "<nick> [<duration> :<reason>]";
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        /* syntax: svshold nickname time :reason goes here */
        /* 'time' is a human-readable timestring, like 2d3h2s. */

        if (!user->server->IsULine()) {
            /* don't allow SVSHOLD from non-ulined clients */
            return CMD_FAILURE;
        }

        if (parameters.size() == 1) {
            std::string reason;

            if (ServerInstance->XLines->DelLine(parameters[0].c_str(), "SVSHOLD", reason,
                                                user)) {
                if (!silent) {
                    ServerInstance->SNO->WriteToSnoMask('x', "%s removed SVSHOLD on %s: %s",
                                                        user->nick.c_str(), parameters[0].c_str(), reason.c_str());
                }
            } else {
                user->WriteNotice("*** SVSHOLD " + parameters[0] + " not found on the list.");
            }
        } else {
            if (parameters.size() < 3) {
                return CMD_FAILURE;
            }

            unsigned long duration;
            if (!InspIRCd::Duration(parameters[1], duration)) {
                user->WriteNotice("*** Invalid duration for SVSHOLD.");
                return CMD_FAILURE;
            }
            SVSHold* r = new SVSHold(ServerInstance->Time(), duration, user->nick,
                                     parameters[2], parameters[0]);

            if (ServerInstance->XLines->AddLine(r, user)) {
                if (silent) {
                    return CMD_SUCCESS;
                }

                if (!duration) {
                    ServerInstance->SNO->WriteToSnoMask('x',
                                                        "%s added a permanent SVSHOLD on %s: %s", user->nick.c_str(),
                                                        parameters[0].c_str(), parameters[2].c_str());
                } else {
                    ServerInstance->SNO->WriteToSnoMask('x',
                                                        "%s added a timed SVSHOLD on %s, expires in %s (on %s): %s",
                                                        user->nick.c_str(), parameters[0].c_str(),
                                                        InspIRCd::DurationString(duration).c_str(),
                                                        InspIRCd::TimeString(ServerInstance->Time() + duration).c_str(),
                                                        parameters[2].c_str());
                }
            } else {
                delete r;
                return CMD_FAILURE;
            }
        }

        return CMD_SUCCESS;
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        return ROUTE_BROADCAST;
    }
};

class ModuleSVSHold : public Module, public Stats::EventListener {
    CommandSvshold cmd;
    SVSHoldFactory s;


  public:
    ModuleSVSHold()
        : Stats::EventListener(this)
        , cmd(this) {
    }

    void init() CXX11_OVERRIDE {
        ServerInstance->XLines->RegisterFactory(&s);
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("svshold");
        silent = tag->getBool("silent", true);
    }

    ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE {
        if (stats.GetSymbol() != 'S') {
            return MOD_RES_PASSTHRU;
        }

        ServerInstance->XLines->InvokeStats("SVSHOLD", stats);
        return MOD_RES_DENY;
    }

    ModResult OnUserPreNick(LocalUser* user,
                            const std::string& newnick) CXX11_OVERRIDE {
        XLine *rl = ServerInstance->XLines->MatchesLine("SVSHOLD", newnick);

        if (rl) {
            user->WriteNumeric(ERR_ERRONEUSNICKNAME, newnick,
                               InspIRCd::Format("Services reserved nickname: %s", rl->reason.c_str()));
            return MOD_RES_DENY;
        }

        return MOD_RES_PASSTHRU;
    }

    ~ModuleSVSHold() {
        ServerInstance->XLines->DelAll("SVSHOLD");
        ServerInstance->XLines->UnregisterFactory(&s);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /SVSHOLD command which allows services to reserve nicknames.", VF_COMMON | VF_VENDOR);
    }
};

MODULE_INIT(ModuleSVSHold)
