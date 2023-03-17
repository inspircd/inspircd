/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 Attila Molnar <attilamolnar@hush.com>
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
    ERR_INPUTTOOLONG = 417
};

class RFCSerializer : public ClientProtocol::Serializer {

    /** The maximum size of client-originated message tags in an incoming message including the `@`. */
    static const std::string::size_type MAX_CLIENT_MESSAGE_TAG_LENGTH = 4095;

    /** The maximum size of server-originated message tags in an outgoing message including the `@`. */
    static const std::string::size_type MAX_SERVER_MESSAGE_TAG_LENGTH = 4095;

    static void SerializeTags(const ClientProtocol::TagMap& tags,
                              const ClientProtocol::TagSelection& tagwl, std::string& line);

  public:
    RFCSerializer(Module* mod)
        : ClientProtocol::Serializer(mod, "rfc") {
    }

    bool Parse(LocalUser* user, const std::string& line,
               ClientProtocol::ParseOutput& parseoutput) CXX11_OVERRIDE;
    ClientProtocol::SerializedMessage Serialize(const ClientProtocol::Message& msg,
            const ClientProtocol::TagSelection& tagwl) const CXX11_OVERRIDE;
};

bool RFCSerializer::Parse(LocalUser* user, const std::string& line,
                          ClientProtocol::ParseOutput& parseoutput) {
    size_t start = line.find_first_not_of(' ');
    if (start == std::string::npos) {
        // Discourage the user from flooding the server.
        user->CommandFloodPenalty += 2000;
        return false;
    }

    // Work out how long the message can actually be.
    size_t maxline = ServerInstance->Config->Limits.MaxLine - start - 2;
    if (line[start] == '@') {
        maxline += MAX_CLIENT_MESSAGE_TAG_LENGTH + 1;
    }

    irc::tokenstream tokens(line, start, maxline);
    ServerInstance->Logs->Log("USERINPUT", LOG_RAWIO, "C[%s] I %s",
                              user->uuid.c_str(), tokens.GetMessage().c_str());

    // This will always exist because of the check at the start of the function.
    std::string token;
    tokens.GetMiddle(token);
    if (token[0] == '@') {
        // Check that the client tags fit within the client tag space.
        if (token.length() > MAX_CLIENT_MESSAGE_TAG_LENGTH) {
            user->WriteNumeric(ERR_INPUTTOOLONG, "Input line was too long");
            user->CommandFloodPenalty += 2000;
            return false;
        }

        // Truncate the RFC part of the message if it is too long.
        size_t maxrfcline = token.length() + ServerInstance->Config->Limits.MaxLine - 1;
        if (tokens.GetMessage().length() > maxrfcline) {
            tokens.GetMessage().erase(maxrfcline);
        }

        // Line begins with message tags, parse them.
        std::string tagval;
        irc::sepstream ss(token.substr(1), ';');
        while (ss.GetToken(token)) {
            // Two or more tags with the same key must not be sent, but if a client violates that we accept
            // the first occurrence of duplicate tags and ignore all later occurrences.
            //
            // Another option is to reject the message entirely but there is no standard way of doing that.
            const std::string::size_type p = token.find('=');
            if (p != std::string::npos) {
                // Tag has a value
                tagval.assign(token, p+1, std::string::npos);
                token.erase(p);
            } else {
                tagval.clear();
            }

            HandleTag(user, token, tagval, parseoutput.tags);
        }

        // Try to read the prefix or command name.
        if (!tokens.GetMiddle(token)) {
            // Discourage the user from flooding the server.
            user->CommandFloodPenalty += 2000;
            return false;
        }
    }

    if (token[0] == ':') {
        // If this exists then the client sent a prefix as part of their
        // message. Section 2.3 of RFC 1459 technically says we should only
        // allow the nick of the client here but in practise everyone just
        // ignores it so we will copy them.

        // Try to read the command name.
        if (!tokens.GetMiddle(token)) {
            // Discourage the user from flooding the server.
            user->CommandFloodPenalty += 2000;
            return false;
        }
    }

    parseoutput.cmd.assign(token);

    // Build the parameter map. We intentionally do not respect the RFC 1459
    // thirteen parameter limit here.
    while (tokens.GetTrailing(token)) {
        parseoutput.params.push_back(token);
    }

    return true;
}

namespace {
void CheckTagLength(std::string& line, size_t prevsize, size_t& length,
                    size_t maxlength) {
    const std::string::size_type diffsize = line.size() - prevsize;
    if (length + diffsize > maxlength) {
        line.erase(prevsize);
    } else {
        length += diffsize;
    }
}
}

void RFCSerializer::SerializeTags(const ClientProtocol::TagMap& tags,
                                  const ClientProtocol::TagSelection& tagwl, std::string& line) {
    size_t client_tag_length = 0;
    size_t server_tag_length = 0;
    for (ClientProtocol::TagMap::const_iterator i = tags.begin(); i != tags.end();
            ++i) {
        if (!tagwl.IsSelected(tags, i)) {
            continue;
        }

        const std::string::size_type prevsize = line.size();
        line.push_back(prevsize ? ';' : '@');
        line.append(i->first);
        const std::string& val = i->second.value;
        if (!val.empty()) {
            line.push_back('=');
            line.append(val);
        }

        // The tags part of the message must not contain more client and server tags than allowed by the
        // message tags specification. This is complicated by the tag space having separate limits for
        // both server-originated and client-originated tags. If either of the tag limits is exceeded then
        // the most recently added tag is removed.
        if (i->first[0] == '+') {
            CheckTagLength(line, prevsize, client_tag_length,
                           MAX_CLIENT_MESSAGE_TAG_LENGTH);
        } else {
            CheckTagLength(line, prevsize, server_tag_length,
                           MAX_SERVER_MESSAGE_TAG_LENGTH);
        }
    }

    if (!line.empty()) {
        line.push_back(' ');
    }
}

ClientProtocol::SerializedMessage RFCSerializer::Serialize(
    const ClientProtocol::Message& msg,
    const ClientProtocol::TagSelection& tagwl) const {
    std::string line;
    SerializeTags(msg.GetTags(), tagwl, line);

    // Save position for length calculation later
    const std::string::size_type rfcmsg_begin = line.size();

    if (msg.GetSource()) {
        line.push_back(':');
        line.append(*msg.GetSource());
        line.push_back(' ');
    }
    line.append(msg.GetCommand());

    const ClientProtocol::Message::ParamList& params = msg.GetParams();
    if (!params.empty()) {
        for (ClientProtocol::Message::ParamList::const_iterator i = params.begin();
                i != params.end()-1; ++i) {
            const std::string& param = *i;
            line.push_back(' ');
            line.append(param);
        }

        line.append(" :", 2).append(params.back());
    }

    // Truncate if too long
    std::string::size_type maxline = ServerInstance->Config->Limits.MaxLine - 2;
    if (line.length() - rfcmsg_begin > maxline) {
        line.erase(rfcmsg_begin + maxline);
    }

    line.append("\r\n", 2);
    return line;
}

class ModuleCoreRFCSerializer : public Module {
    RFCSerializer rfcserializer;

  public:
    ModuleCoreRFCSerializer()
        : rfcserializer(this) {
    }

    void OnCleanup(ExtensionItem::ExtensibleType type,
                   Extensible* item) CXX11_OVERRIDE {
        if (type != ExtensionItem::EXT_USER) {
            return;
        }

        LocalUser* const user = IS_LOCAL(static_cast<User*>(item));
        if ((user) && (user->serializer == &rfcserializer)) {
            ServerInstance->Users.QuitUser(user, "Protocol serializer module unloading");
        }
    }

    void OnUserInit(LocalUser* user) CXX11_OVERRIDE {
        if (!user->serializer) {
            user->serializer = &rfcserializer;
        }
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("RFC client protocol serializer and unserializer", VF_CORE|VF_VENDOR);
    }
};

MODULE_INIT(ModuleCoreRFCSerializer)
