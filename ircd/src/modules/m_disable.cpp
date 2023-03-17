/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
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

enum {
    // From ircu.
    ERR_DISABLED = 517
};

// Holds a list of disabled commands.
typedef std::vector<std::string> CommandList;

// Holds whether modes are disabled or not.
typedef std::bitset<64> ModeStatus;

class ModuleDisable : public Module {
  private:
    CommandList commands;
    ModeStatus chanmodes;
    bool fakenonexistent;
    bool notifyopers;
    ModeStatus usermodes;

    void ReadModes(ConfigTag* tag, const std::string& field, ModeType type,
                   ModeStatus& status) {
        const std::string modes = tag->getString(field);
        for (std::string::const_iterator iter = modes.begin(); iter != modes.end();
                ++iter) {
            const char& chr = *iter;

            // Check that the character is a valid mode letter.
            if (!ModeParser::IsModeChar(chr))
                throw ModuleException(
                    InspIRCd::Format("Invalid mode '%c' was specified in <disabled:%s> at %s",
                                     chr, field.c_str(), tag->getTagLocation().c_str()));

            // Check that the mode actually exists.
            ModeHandler* mh = ServerInstance->Modes->FindMode(chr, type);
            if (!mh)
                throw ModuleException(
                    InspIRCd::Format("Nonexistent mode '%c' was specified in <disabled:%s> at %s",
                                     chr, field.c_str(), tag->getTagLocation().c_str()));

            // Disable the mode.
            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                      "The %c (%s) %s mode has been disabled",
                                      mh->GetModeChar(), mh->name.c_str(),
                                      type == MODETYPE_CHANNEL ? "channel" : "user");
            status.set(chr - 'A');
        }
    }

    void WriteLog(const char* message, ...) CUSTOM_PRINTF(2, 3) {
        std::string buffer;
        VAFORMAT(buffer, message, message);

        if (notifyopers) {
            ServerInstance->SNO->WriteToSnoMask('a', buffer);
        } else {
            ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, buffer);
        }
    }

  public:
    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("disabled");

        // Parse the disabled commands.
        CommandList newcommands;
        irc::spacesepstream commandlist(tag->getString("commands"));
        for (std::string command; commandlist.GetToken(command); ) {
            // Check that the command actually exists.
            Command* handler = ServerInstance->Parser.GetHandler(command);
            if (!handler)
                throw ModuleException(
                    InspIRCd::Format("Nonexistent command '%s' was specified in <disabled:commands> at %s",
                                     command.c_str(), tag->getTagLocation().c_str()));

            // Prevent admins from disabling MODULES for transparency reasons.
            if (handler->name == "MODULES") {
                continue;
            }

            // Disable the command.
            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                      "The %s command has been disabled", handler->name.c_str());
            newcommands.push_back(handler->name);
        }

        // Parse the disabled channel modes.
        ModeStatus newchanmodes;
        ReadModes(tag, "chanmodes", MODETYPE_CHANNEL, newchanmodes);

        // Parse the disabled user modes.
        ModeStatus newusermodes;
        ReadModes(tag, "usermodes", MODETYPE_USER, newusermodes);

        // The server config was valid so we can use these now.
        chanmodes = newchanmodes;
        usermodes = newusermodes;
        commands.swap(newcommands);

        // Whether we should fake the non-existence of disabled things.
        fakenonexistent = tag->getBool("fakenonexistent", tag->getBool("fakenonexistant"));

        // Whether to notify server operators via snomask `a` about the attempted use of disabled commands/modes.
        notifyopers = tag->getBool("notifyopers");
    }

    ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters,
                           LocalUser* user, bool validated) CXX11_OVERRIDE {
        // If a command is unvalidated or the source is not registered we do nothing.
        if (!validated || user->registered != REG_ALL) {
            return MOD_RES_PASSTHRU;
        }

        // If the command is not disabled or the user has the servers/use-disabled-commands priv we do nothing.
        if (!stdalgo::isin(commands, command) || user->HasPrivPermission("servers/use-disabled-commands")) {
            return MOD_RES_PASSTHRU;
        }

        // The user has tried to execute a disabled command!
        user->CommandFloodPenalty += 2000;
        WriteLog("%s was blocked from executing the disabled %s command", user->GetFullRealHost().c_str(), command.c_str());

        if (fakenonexistent) {
            // The server administrator has specified that disabled commands should be
            // treated as if they do not exist.
            user->WriteNumeric(ERR_UNKNOWNCOMMAND, command, "Unknown command");
            ServerInstance->stats.Unknown++;
            return MOD_RES_DENY;
        }

        // Inform the user that the command they executed has been disabled.
        user->WriteNumeric(ERR_DISABLED, command, "Command disabled");
        return MOD_RES_DENY;
    }

    ModResult OnRawMode(User* user, Channel* chan, ModeHandler* mh,
                        const std::string& param, bool adding) CXX11_OVERRIDE {
        // If a mode change is remote or the source is not registered we do nothing.
        if (!IS_LOCAL(user) || user->registered != REG_ALL) {
            return MOD_RES_PASSTHRU;
        }

        // If the mode is not disabled or the user has the servers/use-disabled-modes priv we do nothing.
        const std::bitset<64>& disabled = (mh->GetModeType() == MODETYPE_CHANNEL) ? chanmodes : usermodes;
        if (!disabled.test(mh->GetModeChar() - 'A') || user->HasPrivPermission("servers/use-disabled-modes")) {
            return MOD_RES_PASSTHRU;
        }

        // The user has tried to change a disabled mode!
        const char* what = mh->GetModeType() == MODETYPE_CHANNEL ? "channel" : "user";
        WriteLog("%s was blocked from %ssetting the disabled %s mode %c (%s)",
                 user->GetFullRealHost().c_str(), adding ? "" : "un",
                 what, mh->GetModeChar(), mh->name.c_str());

        if (fakenonexistent) {
            // The server administrator has specified that disabled modes should be
            // treated as if they do not exist.
            int numeric = (mh->GetModeType() == MODETYPE_CHANNEL ? ERR_UNKNOWNMODE :
                           ERR_UNKNOWNSNOMASK);
            const char* typestr = (mh->GetModeType() == MODETYPE_CHANNEL ? "channel" :
                                   "user");
            user->WriteNumeric(numeric, mh->GetModeChar(),
                               InspIRCd::Format("is not a recognised %s mode.", typestr));
            return MOD_RES_DENY;
        }

        // Inform the user that the mode they changed has been disabled.
        user->WriteNumeric(ERR_NOPRIVILEGES, InspIRCd::Format("Permission Denied - %s mode %c (%s) is disabled",
                           what, mh->GetModeChar(), mh->name.c_str()));
        return MOD_RES_DENY;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows commands, channel modes, and user modes to be disabled.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleDisable)
