/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Sadie Powell <sadie@witchery.services>
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
/// $ModConfig: <messagelength modechar="W">
/// $ModDesc: Adds a channel mode which limits the length of messages.
/// $ModDepends: core 3

#include "inspircd.h"

class MessageLengthMode : public ParamMode<MessageLengthMode, LocalIntExt> {
  public:
    MessageLengthMode(Module* Creator)
        : ParamMode<MessageLengthMode, LocalIntExt>(Creator, "message-length",
                ServerInstance->Config->ConfValue("messagelength")->getString("modechar", "W",
                        1, 1)[0]) {
#if defined INSPIRCD_VERSION_SINCE && INSPIRCD_VERSION_SINCE(3, 2)
        syntax = "<max-length>";
#endif
    }

    ModeAction OnSet(User* source, Channel* channel, std::string& parameter) {
        size_t length = ConvToNum<size_t>(parameter);
        if (length == 0 || length > ServerInstance->Config->Limits.MaxLine) {
            source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
            return MODEACTION_DENY;
        }

        this->ext.set(channel, length);
        return MODEACTION_ALLOW;
    }

    void SerializeParam(Channel* channel, int n, std::string& out) {
        out += ConvToStr(n);
    }
};

class ModuleMessageLength : public Module {
  private:
    MessageLengthMode mode;

  public:
    ModuleMessageLength()
        : mode(this) {
    }

    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        if (target.type != MessageTarget::TYPE_CHANNEL) {
            return MOD_RES_PASSTHRU;
        }

        Channel* channel = target.Get<Channel>();
        if (!channel->IsModeSet(&mode)) {
            return MOD_RES_PASSTHRU;
        }

        unsigned int msglength = mode.ext.get(channel);
        if (details.text.length() > msglength) {
            details.text.resize(msglength);
        }

        return MOD_RES_PASSTHRU;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds a channel mode which limits the length of messages.", VF_COMMON);
    }
};

MODULE_INIT(ModuleMessageLength)

