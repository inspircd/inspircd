/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020 Matt Schatz <genius3000@g3k.solutions>
 *
 * This file is a module for InspIRCd.  InspIRCd is free software: you can
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

/// $ModAuthor: genius3000
/// $ModAuthorMail: genius3000@g3k.solutions
/// $ModDepends: core 3
/// $ModDesc: X-line management with XCOPY, XCOUNT, XREMOVE, and XSEARCH

/* XCOUNT, XREMOVE, and XSEARCH are the same except for the end action:
 * XCOUNT will just return a count of matching X-lines.
 * XREMOVE will remove the matching X-lines and return that count.
 * XSEARCH will list the matching X-lines and that count.
 * Syntax is argument and value style "-arg=val" and is the same for each.
 * All arguments are optional:
 * -type=<xline type or *>
 * -mask=<mask> CIDR supported
 * -reason=<reason> Spaces allowed
 * -source=<source> Nick or server ("setby" can be used instead of "source")
 * -set=<time string> (time ago) Prefix with '-' for less than
 * -duration=<time string> (actual set duration) Exact match, prefix '+' for longer than, or '-' for shorter than
 * -expires=<time string> (time ahead) Prefix with '+' for more than
 * -config=<yes|no> Only match or don't match config lines
 *
 * XCOPY copies a specified X-line (by type and mask) to a
 * new X-line of same type with a new mask.
 * Original reason and expiry time are copied but can be
 * overridden with the 'duration' or 'reason' arguments.
 *
 * Note: Both XCOPY and XREMOVE will check for the appropriate
 * command permissions before acting.
 */

/* Helpop Lines for the COPER section
 * Find: '<helpop key="coper" title="Oper Commands" value="'
 * Add 'XCOPY XCOUNT XREMOVE XSEARCH' before 'ZLINE'
 * and space accordingly to match.
 * Find: '<helpop key="kline" ...'
 * Place just above that line:
<helpop key="xcopy" title="/XCOPY <X-line type> <old mask> <new mask> [-duration=<> -reason=<>]" value="
Copies the specified X-line (if found) to a new X-line. The original reason and expiry time
are copied unless overridden with '-duration=' or '-reason='
">

<helpop key="xcount" title="/XCOUNT -type=<X-line type|*> -mask=[!]<m> -reason=[!]<r> -source=[!]<s> -set=[-]<t> -duration=[-+]<t> -expires=[+]<t> -config=<yes|no>" value="
Returns a count of matching X-lines of the specified type (or all types). Mask(m) supports CIDR, reason(r) can include spaces, source(s)
is a nick or server, config (yes or no) will either match only config lines or none. 'Prefix your value for these 3 arguments with a '!' to negate
the match. 't' is a time-string (seconds or 1y2w3d4h5m6s) and can be prefixed with '+' or '-' to adjust matching. All arguments are optional.
">

<helpop key="xremove" title="/XREMOVE -type=<X-line type|*> -mask=[!]<m> -reason=[!]<r> -source=[!]<s> -set=[-]<t> -duration=[-+]<t> -expires=[+]<t> -config=<yes|no>" value="
Removes matching X-lines of the specified type (or all types). Mask(m) supports CIDR, reason(r) can include spaces, source(s)
is a nick or server, config (yes or no) will either match only config lines or none. 'Prefix your value for these 3 arguments with a '!' to negate
the match. 't' is a time-string (seconds or 1y2w3d4h5m6s) and can be prefixed with '+' or '-' to adjust matching. All arguments are optional.
Use /XSEARCH to test before removing.
">

<helpop key="xsearch" title="/XSEARCH -type=<X-line type|*> -mask=[!]<m> -reason=[!]<r> -source=[!]<s> -set=[-]<t> -duration=[-+]<t> -expires=[+]<t> -config=<yes|no>" value="
Lists matching X-lines of the specified type (or all types). Mask(m) supports CIDR, reason(r) can include spaces, source(s)
is a nick or server, config (yes or no) will either match only config lines or none. Prefix your value for these 3 arguments with a '!' to negate
the match. 't' is a time-string (seconds or 1y2w3d4h5m6s) and can be prefixed with '+' or '-' to adjust matching. All arguments are optional.
">

 */


#include "inspircd.h"
#include "xline.h"

namespace {
enum MatchType {
    MATCH_ONLY,
    MATCH_NONE,
    MATCH_ANY
};

struct Criteria {
    MatchType config;
    std::string type;
    std::string mask;
    std::string reason;
    std::string source;
    std::string set;
    std::string duration;
    std::string expires;

    Criteria() { }
    Criteria(const std::string& t, const std::string& m, const std::string& r,
             const std::string& s)
        : type(t)
        , mask(m)
        , reason(r)
        , source(s) {
        config = MATCH_ANY;
    }
};

bool HasCommandPermission(LocalUser* user, std::string type) {
    if (type.length() <= 2) {
        type.append("LINE");
    }

    return user->HasCommandPermission(type);
}

bool ProcessArgs(const CommandBase::Params& params, Criteria& args) {
    if (!params.size()) {
        return false;
    }

    // Track arg reason to include space separated params
    bool argreason = false;

    const std::string mconfig("-config=");
    const std::string mtype("-type=");
    const std::string mmask("-mask=");
    const std::string mreason("-reason=");
    const std::string msource("-source=");
    const std::string msetby("-setby=");
    const std::string mset("-set=");
    const std::string mduration("-duration=");
    const std::string mexpires("-expires=");

    for (std::vector<std::string>::const_iterator p = params.begin();
            p != params.end(); ++p) {
        const std::string param = *p;

        if (irc::find(param, mconfig) != std::string::npos) {
            argreason = false;
            const std::string val(param.substr(mconfig.length()));
            if (!val.empty() && (irc::equals(val, "yes") || irc::equals(val, "true"))) {
                args.config = MATCH_ONLY;
            } else if (!val.empty() && (irc::equals(val, "no")
                                        || irc::equals(val, "false"))) {
                args.config = MATCH_NONE;
            }
        } else if (irc::find(param, mtype) != std::string::npos) {
            argreason = false;
            const std::string val(param.substr(mtype.length()));
            args.type = (!val.empty() ? val : "*");
        } else if (irc::find(param, mmask) != std::string::npos) {
            argreason = false;
            const std::string val(param.substr(mmask.length()));
            args.mask = (!val.empty() ? val : "*");
        } else if (irc::find(param, mreason) != std::string::npos) {
            argreason = true;
            const std::string val(param.substr(mreason.length()));
            args.reason = (!val.empty() ? val : "*");
        } else if (irc::find(param, msource) != std::string::npos) {
            argreason = false;
            const std::string val(param.substr(msource.length()));
            args.source = (!val.empty() ? val : "*");
        } else if (irc::find(param, msetby) != std::string::npos) {
            argreason = false;
            const std::string val(param.substr(msetby.length()));
            args.source = (!val.empty() ? val : "*");
        } else if (irc::find(param, mset) != std::string::npos) {
            argreason = false;
            const std::string val(param.substr(mset.length()));
            args.set = ServerInstance->IsValidDuration(val[0] == '-' ? val.substr(
                           1) : val) ? val : "";
        } else if (irc::find(param, mduration) != std::string::npos) {
            argreason = false;
            const std::string val(param.substr(mduration.length()));
            // 0 is a special case here, to match no expires
            if (val == "0") {
                args.duration = val;
            } else {
                bool prefixed = val[0] == '-' || val[0] == '+';
                args.duration = ServerInstance->IsValidDuration(prefixed ? val.substr(
                                    1) : val) ? val : "";
            }
        } else if (irc::find(param, mexpires) != std::string::npos) {
            argreason = false;
            const std::string val(param.substr(mexpires.length()));
            args.expires = ServerInstance->IsValidDuration(val[0] == '+' ? val.substr(
                               1) : val) ? val : "";
        } else {
            if (argreason) {
                args.reason.append(" " + param);
            } else {
                return false;
            }
        }
    }

    return true;
}

const std::string BuildCriteriaStr(const Criteria& args) {
    std::string criteria;
    const std::string sep(", ");

    if (args.mask != "*") {
        criteria.append("Mask: " + args.mask + sep);
    }
    if (args.reason != "*") {
        criteria.append("Reason: " + args.reason + sep);
    }
    if (args.source != "*") {
        criteria.append("Source: " + args.source + sep);
    }
    if (args.config != MATCH_ANY) {
        criteria.append("From Config: " + std::string(args.config == MATCH_ONLY ?
                        "yes" : "no") + sep);
    }
    if (!args.set.empty()) {
        criteria.append("Set: " + args.set + sep);
    }
    if (!args.duration.empty()) {
        criteria.append("Duration: " + args.duration + sep);
    }
    if (!args.expires.empty()) {
        criteria.append("Expires: " + args.expires + sep);
    }

    if (criteria.empty()) {
        criteria.append("No specific criteria");
    }
    // Remove trailing separator
    else if (criteria[criteria.length() - 1] == ' ') {
        criteria.erase(criteria.length() - 2, 2);
    }

    return criteria;
}

const std::string BuildTypeStr(const std::string& type) {
    std::string typestr;

    // Core/Vendor *-lines are single character, some -extras are two
    // Other X-lines look better without -lines
    if (type.length() <= 2) {
        typestr = type + "-line";
    } else {
        typestr = type;
    }

    return typestr;
}
}

class CommandXBase : public SplitCommand {
    void ProcessLines(LocalUser* user, const Criteria& args,
                      const std::string& linetype, XLineLookup* xlines, unsigned int& matched,
                      unsigned int& total, const bool count, const bool remove) {
        total += xlines->size();
        LookupIter safei;
        bool negate;
        bool match;

        for (LookupIter i = xlines->begin(); i != xlines->end(); ) {
            safei = i;
            safei++;

            XLine* xline = i->second;

            // Config X-line matching
            // For legacy purposes, check for a source of '<Config>' as well.
            if ((args.config == MATCH_ONLY && (!xline->from_config
                                               && xline->source != "<Config>"))
                    || (args.config == MATCH_NONE && (xline->from_config
                            || xline->source == "<Config>"))) {
                i = safei;
                continue;
            }

            // CIDR and glob mask matching, with negation
            // MatchCIDR takes the first argument as a non-CIDR address, so run it both ways.
            negate = args.mask[0] == '!';
            match = InspIRCd::MatchCIDR(xline->Displayable(),
                                        (negate ? args.mask.substr(1) : args.mask)) ||
                    InspIRCd::MatchCIDR((negate ? args.mask.substr(1) : args.mask),
                                        xline->Displayable());
            if ((negate && match) || (!negate && !match)) {
                i = safei;
                continue;
            }

            // Glob reason matching, with negation
            negate = args.reason[0] == '!';
            match = InspIRCd::Match(xline->reason,
                                    (negate ? args.reason.substr(1) : args.reason));
            if ((negate && match) || (!negate && !match)) {
                i = safei;
                continue;
            }

            // Glob source matching, with negation
            negate = args.source[0] == '!';
            match = InspIRCd::Match(xline->source,
                                    (negate ? args.source.substr(1) : args.source));
            if ((negate && match) || (!negate && !match)) {
                i = safei;
                continue;
            }

            // Set (time ago): Prefix '-' means less than; no prefix means more than; both match exact (to the second)
            if (!args.set.empty()) {
                bool prefixed = args.set[0] == '-';
                long set = ServerInstance->Time() - ServerInstance->Duration(
                               prefixed ? args.set.substr(1) : args.set);

                if ((prefixed && xline->set_time < set) ||
                        (!prefixed && xline->set_time > set)) {
                    i = safei;
                    continue;
                }
            }

            // Duration: Prefix '+' means longer than; '-' means shorter than; no prefix means exact; '0' matches no expiry
            if (!args.duration.empty()) {
                bool prefixed = args.duration[0] == '+' || args.duration[0] == '-';
                unsigned long duration = ServerInstance->Duration(prefixed ?
                                         args.duration.substr(1) : args.duration);

                if ((xline->duration == 0 && args.duration != "0") ||
                        (args.duration[0] == '+' && xline->duration <= duration) ||
                        (args.duration[0] == '-' && xline->duration >= duration) ||
                        (!prefixed && xline->duration != duration)) {
                    i = safei;
                    continue;
                }
            }

            // Expires (time ahead): Prefix '+' means more than; no prefix means less than; both match exact (to the second)
            if (!args.expires.empty()) {
                bool prefixed = args.expires[0] == '+';
                unsigned long expires = ServerInstance->Time() + ServerInstance->Duration(
                                            prefixed ? args.expires.substr(1) : args.expires);

                if ((xline->duration == 0) ||
                        (prefixed && xline->set_time + xline->duration < expires) ||
                        (!prefixed && xline->set_time + xline->duration > expires)) {
                    i = safei;
                    continue;
                }
            }

            matched++;

            // Skip the rest when just counting matches
            if (count) {
                i = safei;
                continue;
            }

            std::string expires;
            const std::string display = xline->Displayable();
            const std::string duration = (xline->duration == 0 ? "permanent" :
                                          InspIRCd::DurationString(xline->duration));
            const std::string reason = xline->reason;
            const std::string settime = ServerInstance->TimeString(xline->set_time);
            if (xline->duration == 0) {
                expires = "doesn't expire";
            } else
                expires = InspIRCd::Format("expires in %s (on %s)",
                                           InspIRCd::DurationString(xline->expiry - ServerInstance->Time()).c_str(),
                                           ServerInstance->TimeString(xline->expiry).c_str());

            std::string ret;
            if (remove
                    && ServerInstance->XLines->DelLine(display.c_str(), linetype, ret, user)) {
                ServerInstance->SNO->WriteToSnoMask('x', "%s removed %s on %s: %s",
                                                    user->nick.c_str(),
                                                    BuildTypeStr(linetype).c_str(), display.c_str(), reason.c_str());
            } else if (!remove) {
                user->WriteNotice(
                    InspIRCd::Format("%s on %s set by %s on %s, duration '%s', %s: %s",
                                     BuildTypeStr(linetype).c_str(), display.c_str(), xline->source.c_str(),
                                     settime.c_str(), duration.c_str(), expires.c_str(), reason.c_str()));
            }

            i = safei;
        }
    }

    bool HandleCmd(LocalUser* user, const Criteria& args, Command* cmd) {
        bool count = (cmd->name == "XCOUNT");
        bool remove = (cmd->name == "XREMOVE");
        const std::string action = (remove ? "Removing" : "Listing");
        const std::string criteria = BuildCriteriaStr(args);
        unsigned int matched = 0;
        unsigned int total = 0;

        if (args.type == "*") {
            if (!count)
                user->WriteNotice(InspIRCd::Format("%s matches from all X-line types (%s)",
                                                   action.c_str(), criteria.c_str()));

            std::vector<std::string> xlinetypes = ServerInstance->XLines->GetAllTypes();
            for (std::vector<std::string>::const_iterator x = xlinetypes.begin();
                    x != xlinetypes.end(); ++x) {
                // If removing, check for the command permission for this type
                if (remove && !HasCommandPermission(user, *x)) {
                    user->WriteNotice(
                        InspIRCd::Format("Skipping type '%s' as oper type '%s' does not have access to remove these.",
                                         (*x).c_str(), user->oper->name.c_str()));
                    continue;
                }

                XLineLookup* xlines = ServerInstance->XLines->GetAll(*x);
                if (xlines) {
                    ProcessLines(user, args, *x, xlines, matched, total, count, remove);
                }
            }

            if (count)
                user->WriteNotice(InspIRCd::Format("%u of %u X-lines matched (%s)",
                                                   matched, total, criteria.c_str()));
            else
                user->WriteNotice(InspIRCd::Format("End of list, %u/%u X-lines %s",
                                                   matched, total, (remove ? "removed" : "matched")));
        } else {
            std::string linetype = args.type;
            std::transform(linetype.begin(), linetype.end(), linetype.begin(), ::toupper);

            // Check for the command permission to remove X-lines of this type
            if (remove && !HasCommandPermission(user, linetype)) {
                user->WriteNumeric(ERR_NOPRIVILEGES,
                                   "%s :Permission Denied - Oper type '%s' does not have access to remove X-lines of type '%s'",
                                   user->nick.c_str(), user->oper->name.c_str(), linetype.c_str());
                return false;
            }

            XLineLookup* xlines = ServerInstance->XLines->GetAll(linetype);
            if (!xlines) {
                user->WriteNotice(
                    InspIRCd::Format("Invalid X-line type '%s' (or not yet used X-line)",
                                     linetype.c_str()));
                return false;
            }
            if (xlines->empty()) {
                user->WriteNotice(InspIRCd::Format("No X-lines of type '%s' exist",
                                                   linetype.c_str()));
                return false;
            }

            if (!count)
                user->WriteNotice(InspIRCd::Format("%s matches of X-line type '%s' (%s)",
                                                   action.c_str(), linetype.c_str(), criteria.c_str()));

            ProcessLines(user, args, linetype, xlines, matched, total, count, remove);

            if (count)
                user->WriteNotice(InspIRCd::Format("%u of %u X-lines of type '%s' matched (%s)",
                                                   matched, total, linetype.c_str(), criteria.c_str()));
            else
                user->WriteNotice(InspIRCd::Format("End of list, %u/%u X-lines of type '%s' %s",
                                                   matched, total, linetype.c_str(), (remove ? "removed" : "matched")));
        }

        return true;
    }

  public:
    CommandXBase(Module* Creator,
                 const std::string& cmdname) : SplitCommand(Creator, cmdname, 1) {
        syntax = "-type=<type|*> -mask=[!]<> -reason=[!]<> -source=[!]<> -set=[-]<time> -duration=[-+]<time> -expires=[+]<time> -config=<yes|no>";
        flags_needed = 'o';
    }

    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        if (parameters[0][0] != '-' || parameters[0].find('=') == std::string::npos) {
            user->WriteNotice(InspIRCd::Format("Incorrect argument syntax \"%s\"",
                                               parameters[0].c_str()));
            return CMD_FAILURE;
        }

        // Initialize type, mask, reason, and source with match-all
        Criteria args("*", "*", "*", "*");
        if (!ProcessArgs(parameters, args)) {
            user->WriteNotice("There was a problem processing the given arguments");
            return CMD_FAILURE;
        }

        // Failure message is sent from HandleCmd()
        if (!HandleCmd(user, args, this)) {
            return CMD_FAILURE;
        }

        return CMD_SUCCESS;
    }
};

class CommandXCopy : public SplitCommand {
  public:
    CommandXCopy(Module* Creator) : SplitCommand(Creator, "XCOPY", 3) {
        syntax = "<X-line type> <old mask> <new mask> [-duration=<> -reason=<>]";
        flags_needed = 'o';
    }

    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        const std::string& xtype = parameters[0];
        const std::string& oldmask = parameters[1];
        const std::string& newmask = parameters[2];
        Criteria args;

        if (parameters.size() > 3) {
            CommandBase::Params optional(parameters.begin() + 3, parameters.end());
            if (!ProcessArgs(optional, args)) {
                user->WriteNotice("There was a problem processing the given arguments");
                return CMD_FAILURE;
            }
        }

        // Lookup the X-line matching what we were given
        std::string linetype = xtype;
        std::transform(linetype.begin(), linetype.end(), linetype.begin(), ::toupper);

        // Check for the command permission to copy this type of X-line
        if (!HasCommandPermission(user, linetype)) {
            user->WriteNumeric(ERR_NOPRIVILEGES,
                               "%s :Permission Denied - Oper type '%s' does not have access to copy an X-line of type '%s'",
                               user->nick.c_str(), user->oper->name.c_str(), linetype.c_str());
            return CMD_FAILURE;
        }

        XLineLookup* xlines = ServerInstance->XLines->GetAll(linetype);
        if (!xlines) {
            user->WriteNotice(
                InspIRCd::Format("Invalid X-line type '%s' (or not yet used X-line)",
                                 linetype.c_str()));
            return CMD_FAILURE;
        }

        XLine* oldxline = NULL;
        for (LookupIter i = xlines->begin(); i != xlines->end(); ++i) {
            if (irc::equals(i->second->Displayable(), oldmask)) {
                oldxline = i->second;
                break;
            }
        }

        if (!oldxline) {
            user->WriteNotice(InspIRCd::Format("Could not find \"%s\" in %ss",
                                               oldmask.c_str(), BuildTypeStr(linetype).c_str()));
            return CMD_FAILURE;
        }

        // We don't know the proper mask format for this, so we compare it with the old one
        if ((oldmask.find('!') != std::string::npos && newmask.find('!') == std::string::npos) ||
                (oldmask.find('!') == std::string::npos && newmask.find('!') != std::string::npos) ||
                (oldmask.find('@') != std::string::npos && newmask.find('@') == std::string::npos) ||
                (oldmask.find('@') == std::string::npos && newmask.find('@') != std::string::npos)) {
            user->WriteNotice("Old and new mask must follow the same format (n!u@h or u@h or h)");
            return CMD_FAILURE;
        }

        // Get the needed X-line Factory and create the new X-line
        // Snagged this method from m_xline_db
        XLineFactory* xlf = ServerInstance->XLines->GetFactory(linetype);
        if (!xlf) {
            user->WriteNotice("Great! You just broke the matrix!");
            return CMD_FAILURE;
        }

        unsigned long duration;
        if (!args.duration.empty()) {
            duration = ServerInstance->Duration(args.duration);
        } else {
            duration = (oldxline->duration == 0 ? 0 : (oldxline->set_time +
                        oldxline->duration - ServerInstance->Time()));
        }

        const std::string& reason = (!args.reason.empty() ? args.reason : oldxline->reason);
        const std::string expires = (duration == 0 ? "" : InspIRCd::Format(", expires in %s (on %s)",
                                     InspIRCd::DurationString(duration).c_str(),
                                     InspIRCd::TimeString(ServerInstance->Time() + duration).c_str()));

        XLine* newxline = xlf->Generate(ServerInstance->Time(), duration, user->nick, reason, newmask);
        if (ServerInstance->XLines->AddLine(newxline, user))
            ServerInstance->SNO->WriteToSnoMask('x', "%s added %s %s for %s%s: %s", user->nick.c_str(),
                                                (duration == 0 ? "permanent" : "timed"), BuildTypeStr(linetype).c_str(),
                                                newmask.c_str(), expires.c_str(), reason.c_str());
        else {
            user->WriteNotice(InspIRCd::Format("Failed to add %s on \"%s\"",
                                               BuildTypeStr(linetype).c_str(), newmask.c_str()));
            delete newxline;
            return CMD_FAILURE;
        }

        return CMD_SUCCESS;
    }
};

class ModuleXLineTools : public Module {
    CommandXBase xcount;
    CommandXBase xremove;
    CommandXBase xsearch;
    CommandXCopy xcopy;

  public:
    ModuleXLineTools()
        : xcount(this, "XCOUNT")
        , xremove(this, "XREMOVE")
        , xsearch(this, "XSEARCH")
        , xcopy(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("X-line management tools");
    }
};

MODULE_INIT(ModuleXLineTools)
