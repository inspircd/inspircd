/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2017, 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2017-2018, 2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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
#include "modules/regex.h"
#include "modules/stats.h"
#include "xline.h"

static bool ZlineOnMatch = false;
static bool added_zline = false;

class RLine : public XLine {
  public:
    RLine(time_t s_time, unsigned long d, const std::string& src,
          const std::string& re, const std::string& regexs,
          dynamic_reference<RegexFactory>& rxfactory)
        : XLine(s_time, d, src, re, "R")
        , matchtext(regexs) {
        /* This can throw on failure, but if it does we DONT catch it here, we catch it and display it
         * where the object is created, we might not ALWAYS want it to output stuff to snomask x all the time
         */
        regex = rxfactory->Create(regexs);
    }

    ~RLine() {
        delete regex;
    }

    bool Matches(User* u) CXX11_OVERRIDE {
        LocalUser* lu = IS_LOCAL(u);
        if (lu && lu->exempt) {
            return false;
        }

        const std::string host = u->nick + "!" + u->ident + "@" + u->GetRealHost() + " " + u->GetRealName();
        const std::string ip = u->nick + "!" + u->ident + "@" + u->GetIPString() + " " + u->GetRealName();
        return (regex->Matches(host) || regex->Matches(ip));
    }

    bool Matches(const std::string& compare) CXX11_OVERRIDE {
        return regex->Matches(compare);
    }

    void Apply(User* u) CXX11_OVERRIDE {
        if (ZlineOnMatch) {
            ZLine* zl = new ZLine(ServerInstance->Time(),
                                  duration ? expiry - ServerInstance->Time() : 0,
                                  MODNAME "@" + ServerInstance->Config->ServerName, reason, u->GetIPString());
            if (ServerInstance->XLines->AddLine(zl, NULL)) {
                if (!duration) {
                    ServerInstance->SNO->WriteToSnoMask('x',
                                                        "%s added a permanent Z-line on %s: %s",
                                                        zl->source.c_str(), u->GetIPString().c_str(), zl->reason.c_str());
                } else {
                    ServerInstance->SNO->WriteToSnoMask('x',
                                                        "%s added a timed Z-line on %s, expires in %s (on %s): %s",
                                                        zl->source.c_str(), u->GetIPString().c_str(),
                                                        InspIRCd::DurationString(zl->duration).c_str(),
                                                        InspIRCd::TimeString(zl->duration).c_str(), zl->reason.c_str());
                }
                added_zline = true;
            } else {
                delete zl;
            }
        }
        DefaultApply(u, "R", false);
    }

    const std::string& Displayable() CXX11_OVERRIDE {
        return matchtext;
    }

    std::string matchtext;

    Regex *regex;
};


/** An XLineFactory specialized to generate RLine* pointers
 */
class RLineFactory : public XLineFactory {
  public:
    dynamic_reference<RegexFactory>& rxfactory;
    RLineFactory(dynamic_reference<RegexFactory>& rx) : XLineFactory("R"),
        rxfactory(rx) {
    }

    /** Generate a RLine
     */
    XLine* Generate(time_t set_time, unsigned long duration,
                    const std::string& source, const std::string& reason,
                    const std::string& xline_specific_mask) CXX11_OVERRIDE {
        if (!rxfactory) {
            ServerInstance->SNO->WriteToSnoMask('a',
                                                "Cannot create regexes until engine is set to a loaded provider!");
            throw ModuleException("Regex engine not set or loaded!");
        }

        return new RLine(set_time, duration, source, reason, xline_specific_mask, rxfactory);
    }
};

/** Handle /RLINE
 * Syntax is same as other lines: RLINE regex_goes_here 1d :reason
 */
class CommandRLine : public Command {
    std::string rxengine;
    RLineFactory& factory;

  public:
    CommandRLine(Module* Creator, RLineFactory& rlf) : Command(Creator,"RLINE", 1,
                3), factory(rlf) {
        flags_needed = 'o';
        this->syntax = "<regex> [<duration> :<reason>]";
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {

        if (parameters.size() >= 3) {
            // Adding - XXX todo make this respect <insane> tag perhaps..

            unsigned long duration;
            if (!InspIRCd::Duration(parameters[1], duration)) {
                user->WriteNotice("*** Invalid duration for R-line.");
                return CMD_FAILURE;
            }
            XLine *r = NULL;

            try {
                r = factory.Generate(ServerInstance->Time(), duration, user->nick,
                                     parameters[2], parameters[0]);
            } catch (ModuleException &e) {
                ServerInstance->SNO->WriteToSnoMask('a',
                                                    "Could not add R-line: " + e.GetReason());
            }

            if (r) {
                if (ServerInstance->XLines->AddLine(r, user)) {
                    if (!duration) {
                        ServerInstance->SNO->WriteToSnoMask('x',
                                                            "%s added a permanent R-line on %s: %s", user->nick.c_str(),
                                                            parameters[0].c_str(), parameters[2].c_str());
                    } else {
                        ServerInstance->SNO->WriteToSnoMask('x',
                                                            "%s added a timed R-line on %s, expires in %s (on %s): %s",
                                                            user->nick.c_str(), parameters[0].c_str(),
                                                            InspIRCd::DurationString(duration).c_str(),
                                                            InspIRCd::TimeString(ServerInstance->Time() + duration).c_str(),
                                                            parameters[2].c_str());
                    }

                    ServerInstance->XLines->ApplyLines();
                } else {
                    delete r;
                    user->WriteNotice("*** R-line for " + parameters[0] + " already exists.");
                }
            }
        } else {
            std::string reason;

            if (ServerInstance->XLines->DelLine(parameters[0].c_str(), "R", reason, user)) {
                ServerInstance->SNO->WriteToSnoMask('x', "%s removed R-line on %s: %s",
                                                    user->nick.c_str(), parameters[0].c_str(), reason.c_str());
            } else {
                user->WriteNotice("*** R-line " + parameters[0] + " not found on the list.");
            }
        }

        return CMD_SUCCESS;
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        if (IS_LOCAL(user)) {
            return ROUTE_LOCALONLY;    // spanningtree will send ADDLINE
        }

        return ROUTE_BROADCAST;
    }
};

class ModuleRLine : public Module, public Stats::EventListener {
    dynamic_reference<RegexFactory> rxfactory;
    RLineFactory f;
    CommandRLine r;
    bool MatchOnNickChange;
    bool initing;
    RegexFactory* factory;

  public:
    ModuleRLine()
        : Stats::EventListener(this)
        , rxfactory(this, "regex")
        , f(rxfactory)
        , r(this, f)
        , initing(true) {
    }

    void init() CXX11_OVERRIDE {
        ServerInstance->XLines->RegisterFactory(&f);
    }

    ~ModuleRLine() {
        ServerInstance->XLines->DelAll("R");
        ServerInstance->XLines->UnregisterFactory(&f);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /RLINE command which allows server operators to prevent users matching a nickname!username@hostname+realname regular expression from connecting to the server.", VF_COMMON | VF_VENDOR, rxfactory ? rxfactory->name : "");
    }

    ModResult OnUserRegister(LocalUser* user) CXX11_OVERRIDE {
        // Apply lines on user connect
        XLine *rl = ServerInstance->XLines->MatchesLine("R", user);

        if (rl) {
            // Bang. :P
            rl->Apply(user);
            return MOD_RES_DENY;
        }
        return MOD_RES_PASSTHRU;
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("rline");

        MatchOnNickChange = tag->getBool("matchonnickchange");
        ZlineOnMatch = tag->getBool("zlineonmatch");
        std::string newrxengine = tag->getString("engine");

        factory = rxfactory ? (rxfactory.operator->()) : NULL;

        if (newrxengine.empty()) {
            rxfactory.SetProvider("regex");
        } else {
            rxfactory.SetProvider("regex/" + newrxengine);
        }

        if (!rxfactory) {
            if (newrxengine.empty()) {
                ServerInstance->SNO->WriteToSnoMask('a',
                                                    "WARNING: No regex engine loaded - R-line functionality disabled until this is corrected.");
            } else {
                ServerInstance->SNO->WriteToSnoMask('a',
                                                    "WARNING: Regex engine '%s' is not loaded - R-line functionality disabled until this is corrected.",
                                                    newrxengine.c_str());
            }

            ServerInstance->XLines->DelAll(f.GetType());
        } else if ((!initing) && (rxfactory.operator->() != factory)) {
            ServerInstance->SNO->WriteToSnoMask('a',
                                                "Regex engine has changed, removing all R-lines.");
            ServerInstance->XLines->DelAll(f.GetType());
        }

        initing = false;
    }

    ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE {
        if (stats.GetSymbol() != 'R') {
            return MOD_RES_PASSTHRU;
        }

        ServerInstance->XLines->InvokeStats("R", stats);
        return MOD_RES_DENY;
    }

    void OnUserPostNick(User *user, const std::string &oldnick) CXX11_OVERRIDE {
        if (!IS_LOCAL(user)) {
            return;
        }

        if (!MatchOnNickChange) {
            return;
        }

        XLine *rl = ServerInstance->XLines->MatchesLine("R", user);

        if (rl) {
            // Bang! :D
            rl->Apply(user);
        }
    }

    void OnBackgroundTimer(time_t curtime) CXX11_OVERRIDE {
        if (added_zline) {
            added_zline = false;
            ServerInstance->XLines->ApplyLines();
        }
    }

    void OnUnloadModule(Module* mod) CXX11_OVERRIDE {
        // If the regex engine became unavailable or has changed, remove all R-lines.
        if (!rxfactory) {
            ServerInstance->XLines->DelAll(f.GetType());
        } else if (rxfactory.operator->() != factory) {
            factory = rxfactory.operator->();
            ServerInstance->XLines->DelAll(f.GetType());
        }
    }

    void Prioritize() CXX11_OVERRIDE {
        Module* mod = ServerInstance->Modules->Find("m_cgiirc.so");
        ServerInstance->Modules->SetPriority(this, I_OnUserRegister, PRIORITY_AFTER, mod);
    }
};

MODULE_INIT(ModuleRLine)
