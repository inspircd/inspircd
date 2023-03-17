/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2022 Sadie Powell <sadie@witchery.services>
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

#include "modules/cap.h"

namespace IRCv3 {
namespace Replies {
class CapReference;
class Reply;
class Fail;
class Note;
class Warn;
}
}

/** Reference to the inspircd.org/standard-replies cap. */
class IRCv3::Replies::CapReference
    : public Cap::Reference {
  public:
    CapReference(Module* mod)
        : Cap::Reference(mod, "inspircd.org/standard-replies") {
    }
};

/** Base class for standard replies. */
class IRCv3::Replies::Reply {
  private:
    /** The name of the command for this reply. */
    const std::string cmd;

    /** The event provider for this reply. */
    ClientProtocol::EventProvider evprov;

    /** Wraps a message in an event and sends it to a user.
     * @param user The user to send the message to.
     * @param msg The message to send to the user.
     */
    void SendInternal(LocalUser* user, ClientProtocol::Message& msg) {
        ClientProtocol::Event ev(evprov, msg);
        user->Send(ev);
    }

    void SendNoticeInternal(LocalUser* user, Command* command,
                            const std::string& description) {
        if (command) {
            user->WriteNotice(InspIRCd::Format("*** %s: %s", command->name.c_str(),
                                               description.c_str()));
        } else {
            user->WriteNotice(InspIRCd::Format("*** %s", description.c_str()));
        }
    }

  protected:
    /** Initializes a new instance of the Reply class.
     * @param Creator The module which created this instance.
     * @param Cmd The name of the command to reply with.
     */
    Reply(Module* Creator, const std::string& Cmd)
        : cmd(Cmd)
        , evprov(Creator, Cmd) {
    }

  public:
    /**
     * Sends a standard reply to the specified user.
     * @param user The user to send the reply to.
     * @param command The command that the reply relates to.
     * @param code A machine readable code for this reply.
     * @param description A human readable description of this reply.
     */
    void Send(LocalUser* user, Command* command, const std::string& code,
              const std::string& description) {
        ClientProtocol::Message msg(cmd.c_str(),
                                    ServerInstance->Config->GetServerName());
        if (command) {
            msg.PushParamRef(command->name);
        } else {
            msg.PushParam("*");
        }
        msg.PushParam(code);
        msg.PushParam(description);
        SendInternal(user, msg);
    }

    template<typename T1>
    void Send(LocalUser* user, Command* command, const std::string& code,
              const T1& p1, const std::string& description) {
        ClientProtocol::Message msg(cmd.c_str(),
                                    ServerInstance->Config->GetServerName());
        if (command) {
            msg.PushParamRef(command->name);
        } else {
            msg.PushParam("*");
        }
        msg.PushParam(code);
        msg.PushParam(ConvToStr(p1));
        msg.PushParam(description);
        SendInternal(user, msg);
    }

    template<typename T1, typename T2>
    void Send(LocalUser* user, Command* command, const std::string& code,
              const T1& p1, const T2& p2,
              const std::string& description) {
        ClientProtocol::Message msg(cmd.c_str(),
                                    ServerInstance->Config->GetServerName());
        if (command) {
            msg.PushParamRef(command->name);
        } else {
            msg.PushParam("*");
        }
        msg.PushParam(code);
        msg.PushParam(ConvToStr(p1));
        msg.PushParam(ConvToStr(p2));
        msg.PushParam(description);
        SendInternal(user, msg);
    }

    template<typename T1, typename T2, typename T3>
    void Send(LocalUser* user, Command* command, const std::string& code,
              const T1& p1, const T2& p2,
              const T3& p3, const std::string& description) {
        ClientProtocol::Message msg(cmd.c_str(),
                                    ServerInstance->Config->GetServerName());
        if (command) {
            msg.PushParamRef(command->name);
        } else {
            msg.PushParam("*");
        }
        msg.PushParam(code);
        msg.PushParam(ConvToStr(p1));
        msg.PushParam(ConvToStr(p2));
        msg.PushParam(ConvToStr(p3));
        msg.PushParam(description);
        SendInternal(user, msg);
    }

    template<typename T1, typename T2, typename T3, typename T4>
    void Send(LocalUser* user, Command* command, const std::string& code,
              const T1& p1, const T2& p2,
              const T3& p3, const T4& p4, const std::string& description) {
        ClientProtocol::Message msg(cmd.c_str(),
                                    ServerInstance->Config->GetServerName());
        if (command) {
            msg.PushParamRef(command->name);
        } else {
            msg.PushParam("*");
        }
        msg.PushParam(code);
        msg.PushParam(ConvToStr(p1));
        msg.PushParam(ConvToStr(p2));
        msg.PushParam(ConvToStr(p3));
        msg.PushParam(ConvToStr(p4));
        msg.PushParam(description);
        SendInternal(user, msg);
    }

    template<typename T1, typename T2, typename T3, typename T4, typename T5>
    void Send(LocalUser* user, Command* command, const std::string& code,
              const T1& p1, const T2& p2,
              const T3& p3, const T4& p4, const T5& p5, const std::string& description) {
        ClientProtocol::Message msg(cmd.c_str(),
                                    ServerInstance->Config->GetServerName());
        if (command) {
            msg.PushParamRef(command->name);
        } else {
            msg.PushParam("*");
        }
        msg.PushParam(code);
        msg.PushParam(ConvToStr(p1));
        msg.PushParam(ConvToStr(p2));
        msg.PushParam(ConvToStr(p3));
        msg.PushParam(ConvToStr(p4));
        msg.PushParam(ConvToStr(p5));
        msg.PushParam(description);
        SendInternal(user, msg);
    }

    /**
     * Sends a standard reply to the specified user if they have the specified cap
     * or a notice if they do not.
     * @param user The user to send the reply to.
     * @param cap The capability that determines the type of message to send.
     * @param command The command that the reply relates to.
     * @param code A machine readable code for this reply.
     * @param description A human readable description of this reply.
     */
    void SendIfCap(LocalUser* user, const Cap::Capability& cap, Command* command,
                   const std::string& code,
                   const std::string& description) {
        if (cap.get(user)) {
            Send(user, command, code, description);
        } else {
            SendNoticeInternal(user, command, description);
        }
    }

    template<typename T1>
    void SendIfCap(LocalUser* user, const Cap::Capability& cap, Command* command,
                   const std::string& code,
                   const T1& p1, const std::string& description) {
        if (cap.get(user)) {
            Send(user, command, code, p1, description);
        } else {
            SendNoticeInternal(user, command, description);
        }
    }

    template<typename T1, typename T2>
    void SendIfCap(LocalUser* user, const Cap::Capability& cap, Command* command,
                   const std::string& code,
                   const T1& p1, const T2& p2, const std::string& description) {
        if (cap.get(user)) {
            Send(user, command, code, p1, p2, description);
        } else {
            SendNoticeInternal(user, command, description);
        }
    }

    template<typename T1, typename T2, typename T3>
    void SendIfCap(LocalUser* user, const Cap::Capability& cap, Command* command,
                   const std::string& code,
                   const T1& p1, const T2& p2, const T3& p3, const std::string& description) {
        if (cap.get(user)) {
            Send(user, command, code, p1, p2, p3, description);
        } else {
            SendNoticeInternal(user, command, description);
        }
    }

    template<typename T1, typename T2, typename T3, typename T4>
    void SendIfCap(LocalUser* user, const Cap::Capability& cap, Command* command,
                   const std::string& code,
                   const T1& p1, const T2& p2, const T3& p3, const T4& p4,
                   const std::string& description) {
        if (cap.get(user)) {
            Send(user, command, code, p1, p2, p3, p4, description);
        } else {
            SendNoticeInternal(user, command, description);
        }
    }

    template<typename T1, typename T2, typename T3, typename T4, typename T5>
    void SendIfCap(LocalUser* user, const Cap::Capability& cap, Command* command,
                   const std::string& code,
                   const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5,
                   const std::string& description) {
        if (cap.get(user)) {
            Send(user, command, code, p1, p2, p3, p4, p5, description);
        } else {
            SendNoticeInternal(user, command, description);
        }
    }

    void SendIfCap(LocalUser* user, const Cap::Capability* cap, Command* command,
                   const std::string& code,
                   const std::string& description) {
        if (cap) {
            SendIfCap(user, *cap, command, code, description);
        }
    }

    template<typename T1>
    void SendIfCap(LocalUser* user, const Cap::Capability* cap, Command* command,
                   const std::string& code,
                   const T1& p1, const std::string& description) {
        if (cap) {
            SendIfCap(user, *cap, command, code, p1, description);
        }
    }

    template<typename T1, typename T2>
    void SendIfCap(LocalUser* user, const Cap::Capability* cap, Command* command,
                   const std::string& code,
                   const T1& p1, const T2& p2, const std::string& description) {
        if (cap) {
            SendIfCap(user, *cap, command, code, p1, p2, description);
        }
    }

    template<typename T1, typename T2, typename T3>
    void SendIfCap(LocalUser* user, const Cap::Capability* cap, Command* command,
                   const std::string& code,
                   const T1& p1, const T2& p2, const T3& p3, const std::string& description) {
        if (cap) {
            SendIfCap(user, *cap, command, code, p1, p2, p3, description);
        }
    }

    template<typename T1, typename T2, typename T3, typename T4>
    void SendIfCap(LocalUser* user, const Cap::Capability* cap, Command* command,
                   const std::string& code,
                   const T1& p1, const T2& p2, const T3& p3, const T4& p4,
                   const std::string& description) {
        if (cap) {
            SendIfCap(user, *cap, command, code, p1, p2, p3, p4, description);
        }
    }

    template<typename T1, typename T2, typename T3, typename T4, typename T5>
    void SendIfCap(LocalUser* user, const Cap::Capability* cap, Command* command,
                   const std::string& code,
                   const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5,
                   const std::string& description) {
        if (cap) {
            SendIfCap(user, *cap, command, code, p1, p2, p3, p4, p5, description);
        }
    }
};

/** Sends a FAIL standard reply. */
class IRCv3::Replies::Fail
    : public IRCv3::Replies::Reply {
  public:
    /** Initializes a new instance of the Fail class.
     * @param Creator The module which created this instance.
     */
    Fail(Module* Creator)
        : Reply(Creator, "FAIL") {
    }
};

/** Sends a NOTE standard reply. */
class IRCv3::Replies::Note
    : public IRCv3::Replies::Reply {
  public:
    /** Initializes a new instance of the Note class.
     * @param Creator The module which created this instance.
     */
    Note(Module* Creator)
        : Reply(Creator, "NOTE") {
    }
};

/** Sends a WARN standard reply. */
class IRCv3::Replies::Warn
    : public IRCv3::Replies::Reply {
  public:
    /** Initializes a new instance of the Warn class.
     * @param Creator The module which created this instance.
     */
    Warn(Module* Creator)
        : Reply(Creator, "WARN") {
    }
};
