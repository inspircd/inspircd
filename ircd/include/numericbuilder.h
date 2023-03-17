/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015-2016 Attila Molnar <attilamolnar@hush.com>
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

namespace Numeric {
class WriteNumericSink;
class WriteRemoteNumericSink;

template <char Sep, bool SendEmpty, typename Sink>
class GenericBuilder;

template <char Sep = ',', bool SendEmpty = false>
class Builder;

template <unsigned int NumStaticParams, bool SendEmpty, typename Sink>
class GenericParamBuilder;

template <unsigned int NumStaticParams, bool SendEmpty = false>
class ParamBuilder;
}

class Numeric::WriteNumericSink {
    LocalUser* const user;

  public:
    WriteNumericSink(LocalUser* u)
        : user(u) {
    }

    void operator()(Numeric& numeric) const {
        user->WriteNumeric(numeric);
    }
};

class Numeric::WriteRemoteNumericSink {
    User* const user;

  public:
    WriteRemoteNumericSink(User* u)
        : user(u) {
    }

    void operator()(Numeric& numeric) const {
        user->WriteRemoteNumeric(numeric);
    }
};

template <char Sep, bool SendEmpty, typename Sink>
class Numeric::GenericBuilder {
    Sink sink;
    Numeric numeric;
    const std::string::size_type max;

    bool HasRoom(const std::string::size_type additional) const {
        return (numeric.GetParams().back().size() + additional <= max);
    }

  public:
    GenericBuilder(Sink s, unsigned int num, bool addparam = true,
                   size_t additionalsize = 0)
        : sink(s)
        , numeric(num)
        , max(ServerInstance->Config->Limits.MaxLine -
              ServerInstance->Config->GetServerName().size() - additionalsize - 10) {
        if (addparam) {
            numeric.push(std::string());
        }
    }

    Numeric& GetNumeric() {
        return numeric;
    }

    void Add(const std::string& entry) {
        if (!HasRoom(entry.size())) {
            Flush();
        }
        numeric.GetParams().back().append(entry).push_back(Sep);
    }

    void Add(const std::string& entry1, const std::string& entry2) {
        if (!HasRoom(entry1.size() + entry2.size())) {
            Flush();
        }
        numeric.GetParams().back().append(entry1).append(entry2).push_back(Sep);
    }

    void Flush() {
        std::string& data = numeric.GetParams().back();
        if (IsEmpty()) {
            if (!SendEmpty) {
                return;
            }
        } else {
            data.erase(data.size()-1);
        }

        sink(numeric);
        data.clear();
    }

    bool IsEmpty() const {
        return (numeric.GetParams().back().empty());
    }
};

template <char Sep, bool SendEmpty>
class Numeric::Builder : public
    GenericBuilder<Sep, SendEmpty, WriteNumericSink> {
  public:
    Builder(LocalUser* user, unsigned int num, bool addparam = true,
            size_t additionalsize = 0)
        : ::Numeric::GenericBuilder<Sep, SendEmpty, WriteNumericSink>(WriteNumericSink(
                    user), num, addparam, additionalsize + user->nick.size()) {
    }
};

template <unsigned int NumStaticParams, bool SendEmpty, typename Sink>
class Numeric::GenericParamBuilder {
    Sink sink;
    Numeric numeric;
    std::string::size_type currlen;
    std::string::size_type max;

    bool HasRoom(const std::string::size_type additional) const {
        return (currlen + additional <= max);
    }

  public:
    GenericParamBuilder(Sink s, unsigned int num, size_t additionalsize)
        : sink(s)
        , numeric(num)
        , currlen(0)
        , max(ServerInstance->Config->Limits.MaxLine -
              ServerInstance->Config->GetServerName().size() - additionalsize - 10) {
    }

    void AddStatic(const std::string& entry) {
        max -= (entry.length() + 1);
        numeric.GetParams().push_back(entry);
    }

    void Add(const std::string& entry) {
        if (!HasRoom(entry.size())) {
            Flush();
        }

        currlen += entry.size() + 1;
        numeric.GetParams().push_back(entry);
    }

    void Flush() {
        if ((!SendEmpty) && (IsEmpty())) {
            return;
        }

        sink(numeric);
        currlen = 0;
        numeric.GetParams().erase(numeric.GetParams().begin() + NumStaticParams,
                                  numeric.GetParams().end());
    }

    bool IsEmpty() const {
        return (numeric.GetParams().size() <= NumStaticParams);
    }
};

template <unsigned int NumStaticParams, bool SendEmpty>
class Numeric::ParamBuilder : public
    GenericParamBuilder<NumStaticParams, SendEmpty, WriteNumericSink> {
  public:
    ParamBuilder(LocalUser* user, unsigned int num)
        : ::Numeric::GenericParamBuilder<NumStaticParams, SendEmpty, WriteNumericSink>
          (WriteNumericSink(user), num, user->nick.size()) {
    }
};

namespace Numerics {
class CannotSendTo;
class ChannelPrivilegesNeeded;
class InvalidModeParameter;
class NoSuchChannel;
class NoSuchNick;
}

/** Builder for the ERR_CANNOTSENDTOCHAN and ERR_CANTSENDTOUSER numerics. */
class Numerics::CannotSendTo : public Numeric::Numeric {
  public:
    CannotSendTo(Channel* chan, const std::string& message)
        : Numeric(ERR_CANNOTSENDTOCHAN) {
        push(chan->name);
        push(message);
    }

    CannotSendTo(Channel* chan, const std::string& what, ModeHandler* mh)
        : Numeric(ERR_CANNOTSENDTOCHAN) {
        push(chan->name);
        push(InspIRCd::Format("You cannot send %s to this channel whilst the +%c (%s) mode is set.",
                              what.c_str(), mh->GetModeChar(), mh->name.c_str()));
    }

    CannotSendTo(Channel* chan, const std::string& what, char extban,
                 const std::string& extbandesc)
        : Numeric(ERR_CANNOTSENDTOCHAN) {
        push(chan->name);
        push(InspIRCd::Format("You cannot send %s to this channel whilst %s %c: (%s) extban is set matching you.",
                              what.c_str(), strchr("AEIOUaeiou", extban) ? "an" : "a", extban,
                              extbandesc.c_str()));
    }

    CannotSendTo(User* user, const std::string& message)
        : Numeric(ERR_CANTSENDTOUSER) {
        push(user->registered & REG_NICK ? user->nick : "*");
        push(message);
    }

    CannotSendTo(User* user, const std::string& what, ModeHandler* mh,
                 bool self = false)
        : Numeric(ERR_CANTSENDTOUSER) {
        push(user->registered & REG_NICK ? user->nick : "*");
        push(InspIRCd::Format("You cannot send %s to this user whilst %s have the +%c (%s) mode set.",
                              what.c_str(), self ? "you" : "they", mh->GetModeChar(), mh->name.c_str()));
    }
};

/* Builder for the ERR_CHANOPRIVSNEEDED numeric. */
class Numerics::ChannelPrivilegesNeeded : public Numeric::Numeric {
  public:
    ChannelPrivilegesNeeded(Channel* chan, unsigned int rank,
                            const std::string& message)
        : Numeric(ERR_CHANOPRIVSNEEDED) {
        push(chan->name);

        const PrefixMode* pm = ServerInstance->Modes.FindNearestPrefixMode(rank);
        if (pm) {
            push(InspIRCd::Format("You must be a channel %s or higher to %s.",
                                  pm->name.c_str(), message.c_str()));
        } else {
            push(InspIRCd::Format("You do not have the required channel privileges to %s.",
                                  message.c_str()));
        }
    }
};

/* Builder for the ERR_INVALIDMODEPARAM numeric. */
class Numerics::InvalidModeParameter : public Numeric::Numeric {
  private:
    void push_message(ModeHandler* mode, const std::string& message) {
        if (!message.empty()) {
            // The caller has specified their own message.
            push(message);
            return;
        }

        const std::string& syntax = mode->GetSyntax();
        if (!syntax.empty()) {
            // If the mode has a syntax hint we include it in the message.
            push(InspIRCd::Format("Invalid %s mode parameter. Syntax: %s.",
                                  mode->name.c_str(), syntax.c_str()));
        } else {
            // Otherwise, send it without.
            push(InspIRCd::Format("Invalid %s mode parameter.", mode->name.c_str()));
        }
    }

  public:
    InvalidModeParameter(Channel* chan, ModeHandler* mode,
                         const std::string& parameter, const std::string& message = "")
        : Numeric(ERR_INVALIDMODEPARAM) {
        push(chan->name);
        push(mode->GetModeChar());
        push(parameter);
        push_message(mode, message);
    }

    InvalidModeParameter(User* user, ModeHandler* mode,
                         const std::string& parameter, const std::string& message = "")
        : Numeric(ERR_INVALIDMODEPARAM) {
        push(user->registered & REG_NICK ? user->nick : "*");
        push(mode->GetModeChar());
        push(parameter);
        push_message(mode, message);
    }
};

/** Builder for the ERR_NOSUCHCHANNEL numeric. */
class Numerics::NoSuchChannel : public Numeric::Numeric {
  public:
    NoSuchChannel(const std::string& chan)
        : Numeric(ERR_NOSUCHCHANNEL) {
        push(chan.empty() ? "*" : chan);
        push("No such channel");
    }
};

/** Builder for the ERR_NOSUCHNICK numeric. */
class Numerics::NoSuchNick : public Numeric::Numeric {
  public:
    NoSuchNick(const std::string& nick)
        : Numeric(ERR_NOSUCHNICK) {
        push(nick.empty() ? "*" : nick);
        push("No such nick");
    }
};
