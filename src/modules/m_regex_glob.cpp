/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

class GlobRegex : public Regex {
  public:
    GlobRegex(const std::string& rx) : Regex(rx) {
    }

    bool Matches(const std::string& text) CXX11_OVERRIDE {
        return InspIRCd::Match(text, this->regex_string);
    }
};

class GlobFactory : public RegexFactory {
  public:
    Regex* Create(const std::string& expr) CXX11_OVERRIDE {
        return new GlobRegex(expr);
    }

    GlobFactory(Module* m) : RegexFactory(m, "regex/glob") {}
};

class ModuleRegexGlob : public Module {
    GlobFactory gf;
  public:
    ModuleRegexGlob()
        : gf(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the glob regular expression engine which uses the built-in glob matching system.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleRegexGlob)
