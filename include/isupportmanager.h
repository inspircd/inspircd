/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014, 2016 Attila Molnar <attilamolnar@hush.com>
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

/** This class manages the generation and transmission of ISUPPORT. */
class CoreExport ISupportManager {
  private:
    /** The generated lines which are sent to clients. */
    std::vector<Numeric::Numeric> cachedlines;

    /** Escapes an ISUPPORT token value and appends it to the buffer.
     * @param buffer The buffer to append to.
     * @param value An ISUPPORT token value.
     */
    void AppendValue(std::string& buffer, const std::string& value);

  public:
    /** (Re)build the ISUPPORT vector.
     * Called by the core on boot after all modules have been loaded, and every time when a module is loaded
     * or unloaded. Calls the On005Numeric hook, letting modules manipulate the ISUPPORT tokens.
     */
    void Build();

    /** Returns the cached std::vector of ISUPPORT lines.
     * @return A list of Numeric::Numeric objects prepared for sending to users
     */
    const std::vector<Numeric::Numeric>& GetLines() const {
        return cachedlines;
    }

    /** Send the 005 numerics (ISUPPORT) to a user.
     * @param user The user to send the ISUPPORT numerics to
     */
    void SendTo(LocalUser* user);
};
