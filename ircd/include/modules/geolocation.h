/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019, 2022 Sadie Powell <sadie@witchery.services>
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

namespace Geolocation {
class APIBase;
class API;
class Location;
}

class Geolocation::APIBase : public DataProvider {
  public:
    APIBase(Module* parent)
        : DataProvider(parent, "geolocationapi") {
    }

    /** Looks up the location of the specified user.
     * @param user The user to look up the location of.
     * @return Either an instance of the Location class or NULL if no location could be found.
     */
    virtual Location* GetLocation(User* user) = 0;

    /** Looks up the location of the specified IP address.
     * @param sa The IP address to look up the location of.
     * @return Either an instance of the Location class or NULL if no location could be found.
     */
    virtual Location* GetLocation(irc::sockets::sockaddrs& sa) = 0;
};

class Geolocation::API : public dynamic_reference<Geolocation::APIBase> {
  public:
    API(Module* parent)
        : dynamic_reference<Geolocation::APIBase>(parent, "geolocationapi") {
    }
};

class Geolocation::Location : public usecountbase {
  private:
    /** The two character country code for this location. */
    std::string code;

    /** The country name for this location. */
    std::string name;

  public:
    Location(const std::string& Code, const std::string& Name)
        : code(Code)
        , name(Name) {
    }

    /** Retrieves the two character country code for this location. */
    const std::string& GetCode() const {
        return code;
    }

    /** Retrieves the country name for this location. */
    const std::string& GetName() const {
        return name;
    }
};
