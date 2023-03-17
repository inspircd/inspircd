/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020 Sadie Powell <sadie@witchery.services>
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
#include "modules/cap.h"
#include "modules/ircv3_batch.h"

class BatchMessage : public ClientProtocol::Message {
  public:
    BatchMessage(const IRCv3::Batch::Batch& batch, bool start)
        : ClientProtocol::Message("BATCH", ServerInstance->Config->GetServerName()) {
        char c = (start ? '+' : '-');
        PushParam(std::string(1, c) + batch.GetRefTagStr());
        if ((start) && (!batch.GetType().empty())) {
            PushParamRef(batch.GetType());
        }
    }
};

/** Extra structure allocated only for running batches, containing objects only relevant for
 * that specific run of the batch.
 */
struct IRCv3::Batch::BatchInfo {
    /** List of users that have received the batch start message
     */
    std::vector<LocalUser*> users;
    BatchMessage startmsg;
    ClientProtocol::Event startevent;
    BatchMessage endmsg;
    ClientProtocol::Event endevent;

    BatchInfo(ClientProtocol::EventProvider& protoevprov, IRCv3::Batch::Batch& b)
        : startmsg(b, true)
        , startevent(protoevprov, startmsg)
        , endmsg(b, false)
        , endevent(protoevprov, endmsg) {
    }
};

class IRCv3::Batch::ManagerImpl : public Manager {
    typedef std::vector<Batch*> BatchList;

    Cap::Capability cap;
    ClientProtocol::EventProvider protoevprov;
    LocalIntExt batchbits;
    BatchList active_batches;
    bool unloading;

    bool ShouldSendTag(LocalUser* user,
                       const ClientProtocol::MessageTagData& tagdata) CXX11_OVERRIDE {
        if (!cap.get(user)) {
            return false;
        }

        Batch& batch = *static_cast<Batch*>(tagdata.provdata);
        // Check if this is the first message the user is getting that is part of the batch
        const intptr_t bits = batchbits.get(user);
        if (!(bits & batch.GetBit())) {
            // Send the start batch command ("BATCH +reftag TYPE"), remember the user so we can send them a
            // "BATCH -reftag" message later when the batch ends and set the flag we just checked so this is
            // only done once per user per batch.
            batchbits.set(user, (bits | batch.GetBit()));
            batch.batchinfo->users.push_back(user);
            user->Send(batch.batchinfo->startevent);
        }

        return true;
    }

    unsigned int NextFreeId() const {
        if (active_batches.empty()) {
            return 0;
        }
        return active_batches.back()->GetId()+1;
    }

  public:
    ManagerImpl(Module* mod)
        : Manager(mod)
        , cap(mod, "batch")
        , protoevprov(mod, "BATCH")
        , batchbits("batchbits", ExtensionItem::EXT_USER, mod)
        , unloading(false) {
    }

    void Init() {
        // Set batchbits to 0 for all users in case we were reloaded and the previous, now meaningless,
        // batchbits are set on users
        const UserManager::LocalList& users = ServerInstance->Users.GetLocalUsers();
        for (UserManager::LocalList::const_iterator i = users.begin(); i != users.end();
                ++i) {
            LocalUser* const user = *i;
            batchbits.set(user, 0);
        }
    }

    void Shutdown() {
        unloading = true;
        while (!active_batches.empty()) {
            ManagerImpl::End(*active_batches.back());
        }
    }

    void RemoveFromAll(LocalUser* user) {
        const intptr_t bits = batchbits.get(user);

        // User is quitting, remove them from all lists
        for (BatchList::iterator i = active_batches.begin(); i != active_batches.end();
                ++i) {
            Batch& batch = **i;
            // Check the bit first to avoid list scan in case they're not on the list
            if ((bits & batch.GetBit()) != 0) {
                stdalgo::vector::swaperase(batch.batchinfo->users, user);
            }
        }
    }

    void Start(Batch& batch) CXX11_OVERRIDE {
        if (unloading) {
            return;
        }

        if (batch.IsRunning()) {
            return;    // Already started, don't start again
        }

        const size_t id = NextFreeId();
        if (id >= MAX_BATCHES) {
            return;
        }

        batch.Setup(id);
        // Set the manager field which Batch::IsRunning() checks and is also used by AddToBatch()
        // to set the message tag
        batch.manager = this;
        batch.batchinfo = new IRCv3::Batch::BatchInfo(protoevprov, batch);
        batch.batchstartmsg = &batch.batchinfo->startmsg;
        batch.batchendmsg = &batch.batchinfo->endmsg;
        active_batches.push_back(&batch);
    }

    void End(Batch& batch) CXX11_OVERRIDE {
        if (!batch.IsRunning()) {
            return;
        }

        // Mark batch as stopped
        batch.manager = NULL;

        BatchInfo& batchinfo = *batch.batchinfo;
        // Send end batch message to all users who got the batch start message and unset bit so it can be reused
        for (std::vector<LocalUser*>::const_iterator i = batchinfo.users.begin(); i != batchinfo.users.end(); ++i) {
            LocalUser* const user = *i;
            user->Send(batchinfo.endevent);
            batchbits.set(user, batchbits.get(user) & ~batch.GetBit());
        }

        // erase() not swaperase because the reftag generation logic depends on the order of the elements
        stdalgo::erase(active_batches, &batch);
        delete batch.batchinfo;
        batch.batchinfo = NULL;
    }
};

class ModuleIRCv3Batch : public Module {
    IRCv3::Batch::ManagerImpl manager;

  public:
    ModuleIRCv3Batch()
        : manager(this) {
    }

    void init() CXX11_OVERRIDE {
        manager.Init();
    }

    void OnUnloadModule(Module* mod) CXX11_OVERRIDE {
        if (mod == this) {
            manager.Shutdown();
        }
    }

    void OnUserDisconnect(LocalUser* user) CXX11_OVERRIDE {
        // Remove the user from all internal lists
        manager.RemoveFromAll(user);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the IRCv3 batch client capability.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleIRCv3Batch)
