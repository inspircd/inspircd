/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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

#include "numerics.h"

namespace Numeric {
class Numeric;
}

class Numeric::Numeric {
    /** Numeric number
     */
    unsigned int numeric;

    /** Parameters of the numeric
     */
    CommandBase::Params params;

    /** Source server of the numeric, if NULL (the default) then it is the local server
     */
    Server* sourceserver;

  public:
    /** Constructor
     * @param num Numeric number (RPL_*, ERR_*)
     */
    Numeric(unsigned int num)
        : numeric(num)
        , sourceserver(NULL) {
    }

    /** Add a parameter to the numeric. The parameter will be converted to a string first with ConvToStr().
     * @param x Parameter to add
     */
    template <typename T>
    Numeric& push(const T& x) {
        params.push_back(ConvToStr(x));
        return *this;
    }

    /** Set the source server of the numeric. The source server defaults to the local server.
     * @param server Server to set as source
     */
    void SetServer(Server* server) {
        sourceserver = server;
    }

    /** Get the source server of the numeric
     * @return Source server or NULL if the source is the local server
     */
    Server* GetServer() const {
        return sourceserver;
    }

    /** Get the number of the numeric as an unsigned integer
     * @return Numeric number as an unsigned integer
     */
    unsigned int GetNumeric() const {
        return numeric;
    }

    /** Get the parameters of the numeric
     * @return Parameters of the numeric as a const vector of strings
     */
    const CommandBase::Params& GetParams() const {
        return params;
    }

    /** Get the parameters of the numeric
     * @return Parameters of the numeric as a vector of strings
     */
    CommandBase::Params& GetParams() {
        return params;
    }
};
