/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
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

#include "xline.h"

/** Shun class
 */
class Shun : public XLine {
  public:
    /** Create a Shun.
     * @param s_time The set time
     * @param d The duration of the xline
     * @param src The sender of the xline
     * @param re The reason of the xline
     * @param shunmask Mask to match
     */
    Shun(time_t s_time, unsigned long d, const std::string& src,
         const std::string& re, const std::string& shunmask)
        : XLine(s_time, d, src, re, "SHUN")
        , matchtext(shunmask) {
    }

    bool Matches(User* u) CXX11_OVERRIDE {
        LocalUser* lu = IS_LOCAL(u);
        if (lu && lu->exempt) {
            return false;
        }

        if (InspIRCd::Match(u->GetFullHost(), matchtext) || InspIRCd::Match(u->GetFullRealHost(), matchtext) || InspIRCd::Match(u->nick+"!"+u->ident+"@"+u->GetIPString(), matchtext)) {
            return true;
        }

        if (InspIRCd::MatchCIDR(u->GetIPString(), matchtext, ascii_case_insensitive_map)) {
            return true;
        }

        return false;
    }

    bool Matches(const std::string& str) CXX11_OVERRIDE {
        return (matchtext == str);
    }

    const std::string& Displayable() CXX11_OVERRIDE {
        return matchtext;
    }

  private:
    /** Matching mask **/
    std::string matchtext;
};
