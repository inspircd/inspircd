/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDepends: core 3
/// $ModDesc: Adds the /EXMODE command which explains a mode change.


#include "inspircd.h"

class CommandExplainMode : public SplitCommand {
  private:
    void Reply(LocalUser* user, const char* message, ...) CUSTOM_PRINTF(3, 4) {
        std::string buffer;
        VAFORMAT(buffer, message, message);
        user->WriteNotice("*** EXMODE: " + buffer);
    }

  public:
    CommandExplainMode(Module* Creator)
        : SplitCommand(Creator, "EXMODE", 2) {
        allow_empty_last_param = false;
        syntax = "<target> [(+|-)]<modes> [<mode-parameters>]";
    }

    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        // Work out what type of mode change this is.
        ModeType mtype = ServerInstance->IsChannel(parameters[0]) ? MODETYPE_CHANNEL : MODETYPE_USER;

        bool adding = true;
        unsigned long modecount = 0;
        size_t paramindex = 2;
        for (std::string::const_iterator modechr = parameters[1].begin(); modechr != parameters[1].end(); ++modechr) {
            unsigned char modechar = *modechr;
            if (modechar == '+' || modechar == '-') {
                adding = (modechar == '+');
                continue; // Mode direction set.
            }

            modecount++;
            ModeHandler* mh = ServerInstance->Modes->FindMode(modechar, mtype);
            if (!mh) {
                Reply(user,
                      "%c%c is an unknown mode (any following modes may have incorrect parameters).",
                      adding ? '+' : '-', modechar);
                continue; // Invalid mode.
            }

            if (mh->NeedsParam(true)) {
                const char* param = paramindex < parameters.size() ?
                                    parameters[paramindex].c_str() : "\x1Dmissing\x0F";
                paramindex++;

                const char* ptype = "an \x1Dunknown\x0F mode";
                if (mh->IsPrefixMode()) {
                    ptype = "a prefix mode";
                } else if (mh->IsListMode()) {
                    ptype = "a list mode";
                } else if (mh->IsParameterMode()) {
                    ptype = "a parameter mode";
                }

                Reply(user, "%c%c (%s) is %s (parameter: %s).", adding ? '+' : '-', modechar,
                      mh->name.c_str(), ptype, param);
                continue;
            }

            Reply(user, "%c%c (%s) is a flag mode.", adding ? '+' : '-', modechar,
                  mh->name.c_str());
        }

        Reply(user, "%s mode list complete (%lu total).", mtype == MODETYPE_USER ? "user" : "channel", modecount);
        return CMD_SUCCESS;
    }
};

class ModuleExplainMode : public Module {
  private:
    CommandExplainMode cmd;

  public:
    ModuleExplainMode()
        : cmd(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /EXMODE command which explains a mode change.");
    }
};

MODULE_INIT(ModuleExplainMode)
