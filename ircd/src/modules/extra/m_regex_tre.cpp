/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2016-2017, 2019, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
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

/// $CompilerFlags: find_compiler_flags("tre")
/// $LinkerFlags: find_linker_flags("tre" "-ltre")

/// $PackageInfo: require_system("arch") pkgconf tre
/// $PackageInfo: require_system("darwin") pkg-config tre
/// $PackageInfo: require_system("debian") libtre-dev pkg-config
/// $PackageInfo: require_system("ubuntu") libtre-dev pkg-config

#include "inspircd.h"
#include "modules/regex.h"
#include <sys/types.h>
#include <tre/regex.h>

class TRERegex : public Regex {
    regex_t regbuf;

  public:
    TRERegex(const std::string& rx) : Regex(rx) {
        int flags = REG_EXTENDED | REG_NOSUB;
        int errcode;
        errcode = regcomp(&regbuf, rx.c_str(), flags);
        if (errcode) {
            // Get the error string into a std::string. YUCK this involves at least 2 string copies.
            std::string error;
            char* errbuf;
            size_t sz = regerror(errcode, &regbuf, NULL, 0);
            errbuf = new char[sz + 1];
            memset(errbuf, 0, sz + 1);
            regerror(errcode, &regbuf, errbuf, sz + 1);
            error = errbuf;
            delete[] errbuf;
            regfree(&regbuf);
            throw RegexException(rx, error);
        }
    }

    ~TRERegex() {
        regfree(&regbuf);
    }

    bool Matches(const std::string& text)  CXX11_OVERRIDE {
        return (regexec(&regbuf, text.c_str(), 0, NULL, 0) == 0);
    }
};

class TREFactory : public RegexFactory {
  public:
    TREFactory(Module* m) : RegexFactory(m, "regex/tre") {}
    Regex* Create(const std::string& expr) CXX11_OVERRIDE {
        return new TRERegex(expr);
    }
};

class ModuleRegexTRE : public Module {
    TREFactory trf;

  public:
    ModuleRegexTRE() : trf(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the tre regular expression engine which uses the TRE library.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleRegexTRE)
