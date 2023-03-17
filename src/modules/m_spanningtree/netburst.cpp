/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2019, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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
#include "xline.h"
#include "listmode.h"

#include "treesocket.h"
#include "treeserver.h"
#include "main.h"
#include "commands.h"

/**
 * Creates FMODE messages, used only when syncing channels
 */
class FModeBuilder : public CmdBuilder {
    static const size_t maxline = 480;
    std::string params;
    unsigned int modes;
    std::string::size_type startpos;

  public:
    FModeBuilder(Channel* chan)
        : CmdBuilder("FMODE"), modes(0) {
        push(chan->name).push_int(chan->age).push_raw(" +");
        startpos = str().size();
    }

    /** Add a mode to the message
     */
    void push_mode(const char modeletter, const std::string& mask) {
        push_raw(modeletter);
        params.push_back(' ');
        params.append(mask);
        modes++;
    }

    /** Remove all modes from the message
     */
    void clear() {
        content.erase(startpos);
        params.clear();
        modes = 0;
    }

    /** Prepare the message for sending, next mode can only be added after clear()
     */
    const std::string& finalize() {
        return push_raw(params);
    }

    /** Returns true if the given mask can be added to the message, false if the message
     * has no room for the mask
     */
    bool has_room(const std::string& mask) const {
        return ((str().size() + params.size() + mask.size() + 2 <= maxline) &&
                (modes < ServerInstance->Config->Limits.MaxModes));
    }

    /** Returns true if this message is empty (has no modes)
     */
    bool empty() const {
        return (modes == 0);
    }
};

struct TreeSocket::BurstState {
    SpanningTreeProtocolInterface::Server server;
    BurstState(TreeSocket* sock) : server(sock) { }
};

/** This function is called when we want to send a netburst to a local
 * server. There is a set order we must do this, because for example
 * users require their servers to exist, and channels require their
 * users to exist. You get the idea.
 */
void TreeSocket::DoBurst(TreeServer* s) {
    ServerInstance->SNO->WriteToSnoMask('l',
                                        "Bursting to \002%s\002 (Authentication: %s%s).",
                                        s->GetName().c_str(),
                                        capab->auth_fingerprint ? "SSL certificate fingerprint and " : "",
                                        capab->auth_challenge ? "challenge-response" : "plaintext password");
    this->CleanNegotiationInfo();
    this->WriteLine(CmdBuilder("BURST").push_int(ServerInstance->Time()));
    // Introduce all servers behind us
    this->SendServers(Utils->TreeRoot, s);

    BurstState bs(this);
    // Introduce all users
    this->SendUsers(bs);

    // Sync all channels
    const chan_hash& chans = ServerInstance->GetChans();
    for (chan_hash::const_iterator i = chans.begin(); i != chans.end(); ++i) {
        SyncChannel(i->second, bs);
    }

    // Send all xlines
    this->SendXLines();
    FOREACH_MOD_CUSTOM(Utils->Creator->GetSyncEventProvider(),
                       ServerProtocol::SyncEventListener, OnSyncNetwork, (bs.server));
    this->WriteLine(CmdBuilder("ENDBURST"));
    ServerInstance->SNO->WriteToSnoMask('l',
                                        "Finished bursting to \002"+ s->GetName()+"\002.");

    this->burstsent = true;
}

void TreeSocket::SendServerInfo(TreeServer* from) {
    // Send public version string
    this->WriteLine(CommandSInfo::Builder(from, "version", from->GetVersion()));

    // Send full version string that contains more information and is shown to opers
    this->WriteLine(CommandSInfo::Builder(from, "fullversion",
                                          from->GetFullVersion()));

    // Send the raw version string that just contains the base info
    this->WriteLine(CommandSInfo::Builder(from, "rawversion",
                                          from->GetRawVersion()));
}

/** Recursively send the server tree.
 * This is used during network burst to inform the other server
 * (and any of ITS servers too) of what servers we know about.
 * If at any point any of these servers already exist on the other
 * end, our connection may be terminated.
 */
void TreeSocket::SendServers(TreeServer* Current, TreeServer* s) {
    SendServerInfo(Current);

    const TreeServer::ChildServers& children = Current->GetChildren();
    for (TreeServer::ChildServers::const_iterator i = children.begin();
            i != children.end(); ++i) {
        TreeServer* recursive_server = *i;
        if (recursive_server != s) {
            this->WriteLine(CommandServer::Builder(recursive_server));
            /* down to next level */
            this->SendServers(recursive_server, s);
        }
    }
}

/** Send one or more FJOINs for a channel of users.
 * If the length of a single line is too long, it is split over multiple lines.
 */
void TreeSocket::SendFJoins(Channel* c) {
    CommandFJoin::Builder fjoin(c);

    const Channel::MemberMap& ulist = c->GetUsers();
    for (Channel::MemberMap::const_iterator i = ulist.begin(); i != ulist.end();
            ++i) {
        Membership* memb = i->second;
        if (!fjoin.has_room(memb)) {
            // No room for this user, send the line and prepare a new one
            this->WriteLine(fjoin.finalize());
            fjoin.clear();
        }
        fjoin.add(memb);
    }
    this->WriteLine(fjoin.finalize());
}

/** Send all XLines we know about */
void TreeSocket::SendXLines() {
    std::vector<std::string> types = ServerInstance->XLines->GetAllTypes();

    for (std::vector<std::string>::const_iterator it = types.begin();
            it != types.end(); ++it) {
        /* Expired lines are removed in XLineManager::GetAll() */
        XLineLookup* lookup = ServerInstance->XLines->GetAll(*it);

        /* lookup cannot be NULL in this case but a check won't hurt */
        if (lookup) {
            for (LookupIter i = lookup->begin(); i != lookup->end(); ++i) {
                /* Is it burstable? this is better than an explicit check for type 'K'.
                 * We break the loop as NONE of the items in this group are worth iterating.
                 */
                if (!i->second->IsBurstable()) {
                    break;
                }

                this->WriteLine(CommandAddLine::Builder(i->second));
            }
        }
    }
}

void TreeSocket::SendListModes(Channel* chan) {
    FModeBuilder fmode(chan);
    const ModeParser::ListModeList& listmodes =
        ServerInstance->Modes->GetListModes();
    for (ModeParser::ListModeList::const_iterator i = listmodes.begin();
            i != listmodes.end(); ++i) {
        ListModeBase* mh = *i;
        ListModeBase::ModeList* list = mh->GetList(chan);
        if (!list) {
            continue;
        }

        // Add all items on the list to the FMODE, send it whenever it becomes too long
        const char modeletter = mh->GetModeChar();
        for (ListModeBase::ModeList::const_iterator j = list->begin(); j != list->end();
                ++j) {
            const std::string& mask = j->mask;
            if (!fmode.has_room(mask)) {
                // No room for this mask, send the current line as-is then add the mask to a
                // new, empty FMODE message
                this->WriteLine(fmode.finalize());
                fmode.clear();
            }
            fmode.push_mode(modeletter, mask);
        }
    }

    if (!fmode.empty()) {
        this->WriteLine(fmode.finalize());
    }
}

/** Send channel users, topic, modes and global metadata */
void TreeSocket::SyncChannel(Channel* chan, BurstState& bs) {
    SendFJoins(chan);

    // If the topic was ever set, send it, even if it's empty now
    // because a new empty topic should override an old non-empty topic
    if (chan->topicset != 0) {
        this->WriteLine(CommandFTopic::Builder(chan));
    }

    Utils->SendListLimits(chan, this);
    SendListModes(chan);

    for (Extensible::ExtensibleStore::const_iterator i = chan->GetExtList().begin();
            i != chan->GetExtList().end(); i++) {
        ExtensionItem* item = i->first;
        std::string value = item->ToNetwork(chan, i->second);
        if (!value.empty()) {
            this->WriteLine(CommandMetadata::Builder(chan, item->name, value));
        }
    }

    FOREACH_MOD_CUSTOM(Utils->Creator->GetSyncEventProvider(),
                       ServerProtocol::SyncEventListener, OnSyncChannel, (chan, bs.server));
}

void TreeSocket::SyncChannel(Channel* chan) {
    BurstState bs(this);
    SyncChannel(chan, bs);
}

/** Send all users and their state, including oper and away status and global metadata */
void TreeSocket::SendUsers(BurstState& bs) {
    const user_hash& users = ServerInstance->Users->GetUsers();
    for (user_hash::const_iterator u = users.begin(); u != users.end(); ++u) {
        User* user = u->second;
        if (user->registered != REG_ALL) {
            continue;
        }

        this->WriteLine(CommandUID::Builder(user));

        if (user->IsOper()) {
            this->WriteLine(CommandOpertype::Builder(user));
        }

        if (user->IsAway()) {
            this->WriteLine(CommandAway::Builder(user));
        }

        if (user->uniqueusername) { // TODO: convert this to BooleanExtItem in v4.
            this->WriteLine(CommandMetadata::Builder(user, "uniqueusername", "1"));
        }

        const Extensible::ExtensibleStore& exts = user->GetExtList();
        for (Extensible::ExtensibleStore::const_iterator i = exts.begin();
                i != exts.end(); ++i) {
            ExtensionItem* item = i->first;
            std::string value = item->ToNetwork(u->second, i->second);
            if (!value.empty()) {
                this->WriteLine(CommandMetadata::Builder(user, item->name, value));
            }
        }

        FOREACH_MOD_CUSTOM(Utils->Creator->GetSyncEventProvider(),
                           ServerProtocol::SyncEventListener, OnSyncUser, (user, bs.server));
    }
}
