/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2013, 2016, 2018, 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
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

/// $CompilerFlags: -std=c++11


#include "inspircd.h"
#include "modules/regex.h"
#include <regex>

class StdRegex : public Regex {
    std::regex regexcl;

  public:
    StdRegex(const std::string& rx, std::regex::flag_type fltype) : Regex(rx) {
        try {
            regexcl.assign(rx, fltype | std::regex::optimize);
        } catch(const std::regex_error& rxerr) {
            throw RegexException(rx, rxerr.what());
        }
    }

    bool Matches(const std::string& text) CXX11_OVERRIDE {
        return std::regex_search(text, regexcl);
    }
};

class StdRegexFactory : public RegexFactory {
  public:
    std::regex::flag_type regextype;
    StdRegexFactory(Module* m) : RegexFactory(m, "regex/stdregex") {}
    Regex* Create(const std::string& expr) CXX11_OVERRIDE {
        return new StdRegex(expr, regextype);
    }
};

class ModuleRegexStd : public Module {
  public:
    StdRegexFactory ref;
    ModuleRegexStd() : ref(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the stdregex regular expression engine which uses the C++11 std::regex regular expression matching system.", VF_VENDOR);
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* Conf = ServerInstance->Config->ConfValue("stdregex");

        const std::string regextype = Conf->getString("type", "ecmascript", 1);
        if (stdalgo::string::equalsci(regextype, "bre")) {
            ref.regextype = std::regex::basic;
        } else if (stdalgo::string::equalsci(regextype, "ere")) {
            ref.regextype = std::regex::extended;
        } else if (stdalgo::string::equalsci(regextype, "awk")) {
            ref.regextype = std::regex::awk;
        } else if (stdalgo::string::equalsci(regextype, "grep")) {
            ref.regextype = std::regex::grep;
        } else if (stdalgo::string::equalsci(regextype, "egrep")) {
            ref.regextype = std::regex::egrep;
        } else {
            if (!stdalgo::string::equalsci(regextype, "ecmascript")) {
                ServerInstance->SNO->WriteToSnoMask('a',
                                                    "WARNING: Nonexistent regex engine '%s' specified. Falling back to ECMAScript.",
                                                    regextype.c_str());
            }
            ref.regextype = std::regex::ECMAScript;
        }
    }
};

MODULE_INIT(ModuleRegexStd)
