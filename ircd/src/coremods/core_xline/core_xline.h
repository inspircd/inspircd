/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2017-2018, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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

#include "inspircd.h"

class InsaneBan {
  public:
    class MatcherBase {
      public:
        virtual long Run(const std::string& mask) = 0;
    };

    template <typename T>
    class Matcher : public MatcherBase {
      public:
        long Run(const std::string& mask) CXX11_OVERRIDE {
            long matches = 0;
            const T* c = static_cast<T*>(this);
            const user_hash& users = ServerInstance->Users->GetUsers();
            for (user_hash::const_iterator i = users.begin(); i != users.end(); ++i) {
                if (c->Check(i->second, mask)) {
                    matches++;
                }
            }
            return matches;
        }
    };

    class IPHostMatcher : public Matcher<IPHostMatcher> {
      public:
        bool Check(User* user, const std::string& mask) const;
    };

    /** Check if the given mask matches too many users according to the config, send an announcement if yes
     * @param mask A mask to match against
     * @param test The test that determines if a user matches the mask or not
     * @param user A user whose nick will be included in the announcement if one is made
     * @param bantype Type of the ban being set, will be used in the announcement if one is made
     * @param confkey Name of the config key (inside the insane tag) which if false disables any checking
     * @return True if the given mask matches too many users, false if not
     */
    static bool MatchesEveryone(const std::string& mask, MatcherBase& test,
                                User* user, char bantype, const char* confkey);
};

/** Handle /ELINE.
 */
class CommandEline : public Command {
  public:
    /** Constructor for E-line.
     */
    CommandEline(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /GLINE.
 */
class CommandGline : public Command {
  public:
    /** Constructor for G-line.
     */
    CommandGline(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /KLINE.
 */
class CommandKline : public Command {
  public:
    /** Constructor for K-line.
     */
    CommandKline(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /QLINE.
 */
class CommandQline : public Command {
    class NickMatcher : public InsaneBan::Matcher<NickMatcher> {
      public:
        bool Check(User* user, const std::string& mask) const;
    };

  public:
    /** Constructor for Q-line.
     */
    CommandQline(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /ZLINE.
 */
class CommandZline : public Command {
    class IPMatcher : public InsaneBan::Matcher<IPMatcher> {
      public:
        bool Check(User* user, const std::string& mask) const;
    };

  public:
    /** Constructor for Z-line.
     */
    CommandZline(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};
