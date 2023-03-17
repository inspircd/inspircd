/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019, 2021 Sadie Powell <sadie@witchery.services>
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

Serializable::Data& Serializable::Data::Load(const std::string& key,
        std::string& out) {
    EntryMap::iterator iter = this->entries.find(key);
    if (iter == this->entries.end()) {
        ServerInstance->Logs->Log("SERIALIZE", LOG_DEBUG,
                                  "Unable to load missing kv %s!", key.c_str());
    } else {
        out = iter->second;
        ServerInstance->Logs->Log("SERIALIZE", LOG_DEBUG, "Loaded kv %s: %s",
                                  key.c_str(), out.c_str());
    }
    return *this;
}

Serializable::Data& Serializable::Data::Load(const std::string& key,
        Serializable::Data& out) {
    ChildMap::iterator iter = this->children.find(key);
    if (iter == this->children.end()) {
        ServerInstance->Logs->Log("SERIALIZE", LOG_DEBUG,
                                  "Unable to load missing data %s!", key.c_str());
    } else {
        out = iter->second;
        ServerInstance->Logs->Log("SERIALIZE", LOG_DEBUG, "Loaded data: %s",
                                  key.c_str());
    }
    return *this;
}

Serializable::Data& Serializable::Data::Store(const std::string& key,
        const std::string& value) {
    ServerInstance->Logs->Log("SERIALIZE", LOG_DEBUG, "Stored kv %s: %s",
                              key.c_str(), value.c_str());
    this->entries[key] = value;
    return *this;
}

Serializable::Data& Serializable::Data::Store(const std::string& key,
        const Serializable::Data& value) {
    ServerInstance->Logs->Log("SERIALIZE", LOG_DEBUG, "Stored data: %s",
                              key.c_str());
    this->children[key] = value;
    return *this;
}

bool Extensible::Deserialize(Serializable::Data& data) {
    // If the extensible has been culled then it shouldn't be deserialized.
    if (culled) {
        return false;
    }

    const Serializable::Data::EntryMap& entries = data.GetEntries();
    for (Serializable::Data::EntryMap::const_iterator iter = entries.begin();
            iter != entries.end(); ++iter) {
        const std::string& name = iter->first;
        ExtensionItem* item = ServerInstance->Extensions.GetItem(name);
        if (item) {
            item->FromInternal(this, iter->second);
            continue;
        }

        ServerInstance->Logs->Log("SERIALIZE", LOG_DEBUG,
                                  "Tried to deserialize the %s extension item but it doesn't exist",
                                  name.c_str());
    }
    return true;
}

bool Extensible::Serialize(Serializable::Data& data) {
    // If the extensible has been culled then it shouldn't be serialized.
    if (culled) {
        ServerInstance->Logs->Log("SERIALIZE", LOG_DEBUG,
                                  "Tried to serialize an extensible which has been culled");
        return false;
    }

    for (Extensible::ExtensibleStore::const_iterator iter = extensions.begin();
            iter != extensions.end(); ++iter) {
        ExtensionItem* item = iter->first;
        const std::string value = item->ToInternal(this, iter->second);
        if (!value.empty()) {
            data.Store(item->name, value);
        }
    }
    return true;
}

bool User::Deserialize(Serializable::Data& data) {
    // If the user is quitting they shouldn't be deserialized.
    if (quitting) {
        ServerInstance->Logs->Log("SERIALIZE", LOG_DEBUG,
                                  "Tried to deserialize %s who is in the process of quitting",
                                  uuid.c_str());
        return false;
    }

    // Check we're actually deserialising data for this user.
    std::string client_uuid;
    data.Load("uuid", client_uuid);
    if (!client_uuid.empty() && client_uuid != uuid) {
        ServerInstance->Logs->Log("SERIALIZE", LOG_DEBUG,
                                  "Tried to deserialize %s into %s",
                                  client_uuid.c_str(), uuid.c_str());
        return false;
    }

    // Deserialize the extensions first.
    Serializable::Data exts;
    data.Load("extensions", exts);
    if (!Extensible::Deserialize(exts)) {
        return false;
    }

    long client_port;
    bool user_uniqueusername;
    std::string client_addr;
    std::string user_modes;
    std::string user_oper;
    std::string user_snomasks;

    // Apply the members which can be applied directly.
    data.Load("age", age)
    .Load("awaymsg", awaymsg)
    .Load("awaytime", awaytime)
    .Load("client_sa.addr", client_addr)
    .Load("client_sa.port", client_port)
    .Load("displayhost", displayhost)
    .Load("ident", ident)
    .Load("modes", user_modes)
    .Load("nick", nick)
    .Load("oper", user_oper)
    .Load("realhost", realhost)
    .Load("realname", realname)
    .Load("signon", signon)
    .Load("snomasks", user_snomasks)
    .Load("uniqueusername", user_uniqueusername);

    // Apply the rest of the members.
    modes = std::bitset<ModeParser::MODEID_MAX>(user_modes);
    snomasks = std::bitset<64>(user_snomasks);
    uniqueusername = user_uniqueusername;

    ServerConfig::OperIndex::const_iterator iter =
        ServerInstance->Config->OperTypes.find(user_oper);
    if (iter != ServerInstance->Config->OperTypes.end()) {
        oper = iter->second;
    } else {
        oper = new OperInfo(user_oper);
    }

    irc::sockets::sockaddrs sa;
    if (irc::sockets::aptosa(client_addr, client_port, sa)
            || irc::sockets::untosa(client_addr, sa)) {
        client_sa = sa;
    }

    InvalidateCache();
    return true;
}

bool User::Serialize(Serializable::Data& data) {
    // If the user is quitting they shouldn't be serialized.
    if (quitting) {
        ServerInstance->Logs->Log("SERIALIZE", LOG_DEBUG,
                                  "Tried to serialize %s who is in the process of quitting",
                                  uuid.c_str());
        return false;
    }

    // If the user is unregistered they shouldn't be serialised.
    if (registered != REG_ALL) {
        return false;
    }

    // Serialize the extensions first.
    Serializable::Data exts;
    if (!Extensible::Serialize(exts)) {
        return false;
    }
    data.Store("extensions", exts);

    // The following member variables not checked above are not serialised:
    // * cached_fullhost (serialising cache variables is unnecessary)
    // * cached_fullrealhost (serialising cache variables is unnecessary)
    // * cached_hostip (serialising cache variables is unnecessary)
    // * cached_makehost (serialising cache variables is unnecessary)
    // * cachedip (serialising cache variables is unnecessary)
    // * server (specific to the origin server)
    // * usertype (can't be networked reliably)
    data.Store("age", age)
    .Store("awaymsg", awaymsg)
    .Store("awaytime", awaytime)
    .Store("client_sa.addr", client_sa.addr())
    .Store("client_sa.port", client_sa.port())
    .Store("displayhost", displayhost)
    .Store("ident", ident)
    .Store("modes", modes.to_string())
    .Store("nick", nick)
    .Store("oper", oper ? oper->name : "")
    .Store("realhost", realhost)
    .Store("realname", realname)
    .Store("signon", signon)
    .Store("snomasks", snomasks.to_string())
    .Store("uniqueusername", uniqueusername)
    .Store("uuid", uuid);

    return true;
}

bool LocalUser::Deserialize(Serializable::Data& data) {

    // Deserialize the base class first.
    if (!User::Deserialize(data)) {
        return false;
    }

    bool user_exempt;
    bool user_lastping;
    long server_port;
    std::string server_addr;

    // Apply the members which can be applied directly.
    data.Load("bytes_in", bytes_in)
    .Load("bytes_out", bytes_out)
    .Load("cmds_in", cmds_in)
    .Load("cmds_out", cmds_out)
    .Load("CommandFloodPenalty", CommandFloodPenalty)
    .Load("exempt", user_exempt)
    .Load("idle_lastmsg", idle_lastmsg)
    .Load("lastping", user_lastping)
    .Load("nextping", nextping)
    .Load("password", password)
    .Load("server_sa.addr", server_addr)
    .Load("server_sa.port", server_port);

    // Apply the rest of the members.
    irc::sockets::sockaddrs sa;
    if (irc::sockets::aptosa(server_addr, server_port, sa)
            || irc::sockets::untosa(server_addr, sa)) {
        server_sa = sa;
    }

    // These are bitfields so we need to ensure they only get the appropriate bits.
    exempt = user_exempt ? 1 : 0;
    lastping = user_lastping ? 1 : 0;
    return true;
}

bool LocalUser::Serialize(Serializable::Data& data) {
    // Serialize the base class first.
    if (!User::Serialize(data)) {
        return false;
    }

    // The following member variables not checked above are not serialised:
    // * already_sent (can't be networked reliably)
    // * eh (shouldn't be networked)
    // * MyClass (might not be the same on a different server)
    // * serializer (might not be the same on a different connection)
    data.Store("bytes_in", bytes_in)
    .Store("bytes_out", bytes_out)
    .Store("cmds_in", cmds_in)
    .Store("cmds_out", cmds_out)
    .Store("CommandFloodPenalty", CommandFloodPenalty)
    .Store("exempt", exempt)
    .Store("idle_lastmsg", idle_lastmsg)
    .Store("lastping", lastping)
    .Store("nextping", nextping)
    .Store("password", password)
    .Store("server_sa.addr", server_sa.addr())
    .Store("server_sa.port", server_sa.port());
    return true;
}
