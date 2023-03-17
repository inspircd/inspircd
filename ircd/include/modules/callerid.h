/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Sadie Powell <sadie@witchery.services>
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

namespace CallerID {
class APIBase;
class API;
}

class CallerID::APIBase : public DataProvider {
  public:
    APIBase(Module* parent)
        : DataProvider(parent, "m_callerid_api") {
    }

    /** Determines whether \p source is on the accept list of \p target.
     * @param source The user to search for in the accept list.
     * @param target The user who's accept list to search in.
     * @return True if \p source is on \p target's accept list; otherwise, false.
     */
    virtual bool IsOnAcceptList(User* source, User* target) = 0;
};

class CallerID::API : public dynamic_reference<CallerID::APIBase> {
  public:
    API(Module* parent)
        : dynamic_reference<CallerID::APIBase>(parent, "m_callerid_api") {
    }
};
