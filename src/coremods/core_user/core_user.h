/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2020 Sadie Powell <sadie@witchery.services>
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
#include "listmode.h"
#include "modules/away.h"

enum {
    // From RFC 1459.
    ERR_NOORIGIN = 409
};

class MessageWrapper {
    std::string prefix;
    std::string suffix;
    bool fixed;

  public:
    /**
     * Wrap the given message according to the config rules
     * @param message The message to wrap
     * @param out String where the result is placed
     */
    void Wrap(const std::string& message, std::string& out);

    /**
     * Read the settings from the given config keys (options block)
     * @param prefixname Name of the config key to read the prefix from
     * @param suffixname Name of the config key to read the suffix from
     * @param fixedname Name of the config key to read the fixed string string from.
     * If this key has a non-empty value, all messages will be replaced with it.
     */
    void ReadConfig(const char* prefixname, const char* suffixname,
                    const char* fixedname);
};

/** Handle /AWAY.
 */
class CommandAway : public Command {
  private:
    Away::EventProvider awayevprov;

  public:
    /** Constructor for away.
     */
    CommandAway(Module* parent);
    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
    RouteDescriptor GetRouting(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /ISON.
 */
class CommandIson : public SplitCommand {
  public:
    /** Constructor for ison.
     */
    CommandIson(Module* parent)
        : SplitCommand(parent, "ISON", 1) {
        allow_empty_last_param = false;
        syntax = "<nick> [<nick>]+";
    }
    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE;
};


/** Handle /NICK.
 */
class CommandNick : public SplitCommand {
  public:
    /** Constructor for nick.
     */
    CommandNick(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /PART.
 */
class CommandPart : public Command {
  public:
    MessageWrapper msgwrap;

    /** Constructor for part.
     */
    CommandPart(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
    RouteDescriptor GetRouting(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /QUIT.
 */
class CommandQuit : public Command {
  private:
    StringExtItem operquit;

  public:
    MessageWrapper msgwrap;

    /** Constructor for quit.
     */
    CommandQuit(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;

    RouteDescriptor GetRouting(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /USER.
 */
class CommandUser : public SplitCommand {
  public:
    /** Constructor for user.
     */
    CommandUser(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE;

    /** Run the OnUserRegister hook if the user has sent both NICK and USER. Called after an unregistered user
     * successfully executes the USER or the NICK command.
     * @param user User to inspect and possibly pass to the OnUserRegister hook
     * @return CMD_FAILURE if OnUserRegister was called and it returned MOD_RES_DENY, CMD_SUCCESS in every other case
     * (i.e. if the hook wasn't fired because the user still needs to send NICK/USER or if it was fired and finished with
     * a non-MOD_RES_DENY result).
     */
    static CmdResult CheckRegister(LocalUser* user);
};

/** Handle /USERHOST.
 */
class CommandUserhost : public Command {
    UserModeReference hideopermode;

  public:
    /** Constructor for userhost.
     */
    CommandUserhost(Module* parent)
        : Command(parent,"USERHOST", 1)
        , hideopermode(parent, "hideoper") {
        allow_empty_last_param = false;
        syntax = "<nick> [<nick>]+";
    }
    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** User mode +s
 */
class ModeUserServerNoticeMask : public ModeHandler {
    /** Process a snomask modifier string, e.g. +abc-de
     * @param user The target user
     * @param input A sequence of notice mask characters
     * @return The cleaned mode sequence which can be output,
     * e.g. in the above example if masks c and e are not
     * valid, this function will return +ab-d
     */
    std::string ProcessNoticeMasks(User* user, const std::string& input);

  public:
    ModeUserServerNoticeMask(Module* Creator);
    ModeAction OnModeChange(User* source, User* dest, Channel* channel,
                            std::string &parameter, bool adding) CXX11_OVERRIDE;

    /** Create a displayable mode string of the snomasks set on a given user
     * @param user The user whose notice masks to format
     * @return The notice mask character sequence
     */
    std::string GetUserParameter(const User* user) const CXX11_OVERRIDE;
};

/** User mode +o
 */
class ModeUserOperator : public ModeHandler {
  public:
    ModeUserOperator(Module* Creator);
    ModeAction OnModeChange(User* source, User* dest, Channel* channel,
                            std::string &parameter, bool adding) CXX11_OVERRIDE;
};
