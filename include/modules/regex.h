/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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


#pragma once

#include "event.h"

class Regex : public classbase {
  protected:
    /** The uncompiled regex string. */
    std::string regex_string;

    // Constructor may as well be protected, as this class is abstract.
    Regex(const std::string& rx) : regex_string(rx) { }

  public:

    virtual ~Regex() { }

    virtual bool Matches(const std::string& text) = 0;

    const std::string& GetRegexString() const {
        return regex_string;
    }
};

class RegexFactory : public DataProvider {
  public:
    RegexFactory(Module* Creator, const std::string& Name) : DataProvider(Creator,
                Name) { }

    virtual Regex* Create(const std::string& expr) = 0;
};

class RegexException : public ModuleException {
  public:
    RegexException(const std::string& regex, const std::string& error)
        : ModuleException("Error in regex '" + regex + "': " + error) { }

    RegexException(const std::string& regex, const std::string& error, int offset)
        : ModuleException("Error in regex '" + regex + "' at offset " + ConvToStr(
                              offset) + ": " + error) { }
};
