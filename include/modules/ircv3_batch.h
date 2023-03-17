/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020, 2022 Sadie Powell <sadie@witchery.services>
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


#pragma once

// For CapReference
#include "modules/cap.h"

namespace IRCv3 {
namespace Batch {
typedef uint64_t RefTag;
class Manager;
class ManagerImpl;
class Batch;
struct BatchInfo;
class API;
class CapReference;

static const unsigned int MAX_BATCHES = (sizeof(intptr_t) * 8) - 1;
}
}

/** Batch Manager.
 * Implements batch starting and stopping. When it becomes unavailable (due to e.g. module unload)
 * all running batches are stopped.
 */
class IRCv3::Batch::Manager : public DataProvider,
    public ClientProtocol::MessageTagProvider {
  public:
    /** Constructor.
     * @param mod Module that owns the Manager.
     */
    Manager(Module* mod)
        : DataProvider(mod, "batchapi")
        , ClientProtocol::MessageTagProvider(mod) {
    }

    /** Start a batch.
     * Check Batch::IsRunning() to learn if the batch has been started.
     * @param batch Batch to start.
     */
    virtual void Start(Batch& batch) = 0;

    /** End a batch.
     * @param batch Batch to end.
     */
    virtual void End(Batch& batch) = 0;
};

/** Represents a batch.
 * Batches are used to group together physically separate client protocol messages that logically belong
 * together for one reason or another. The type of a batch, if provided, indicates what kind of grouping
 * it does.
 *
 * Batch objects have two states: running and stopped. If a batch is running, messages can be added to it.
 * If a message has been added to a batch and that message is sent to a client that negotiated the batch
 * capability then the client will receive a message tag attached to the message indicating the batch that
 * the message is a part of. If a message M is part of a batch B and M is sent to a client that hasn't yet
 * received any message from batch B it will get a batch start message for B before M. When a batch B is
 * stopped, every client that received at least one message which was in batch B will receive an end of
 * batch message for B.
 * A message may only be part of a single batch at any given time.
 */
class IRCv3::Batch::Batch {
    Manager* manager;
    const std::string type;
    RefTag reftag;
    std::string reftagstr;
    unsigned int bit;
    BatchInfo* batchinfo;
    ClientProtocol::Message* batchstartmsg;
    ClientProtocol::Message* batchendmsg;

    void Setup(unsigned int b) {
        bit = b;
        reftag = (static_cast<RefTag>(1) << bit);
        reftagstr = ConvToStr(reftag);
    }

    unsigned int GetId() const {
        return bit;
    }
    intptr_t GetBit() const {
        return reftag;
    }

  public:
    /** Constructor.
     * The batch is initially stopped. To start it, pass it to Manager::Start().
     * @param Type Batch type string, used to indicate what kind of grouping the batch does. May be empty.
     */
    Batch(const std::string& Type)
        : manager(NULL)
        , type(Type)
        , batchinfo(NULL)
        , batchstartmsg(NULL)
        , batchendmsg(NULL) {
    }

    /** Destructor.
     * If the batch is running, it is ended.
     */
    ~Batch() {
        if (manager) {
            manager->End(*this);
        }
    }

    /** Add a message to the batch.
     * If the batch isn't running then this method does nothing.
     * @param msg Message to add to the batch. If it is already part of any batch, this method is a no-op.
     */
    void AddToBatch(ClientProtocol::Message& msg) {
        if (manager) {
            msg.AddTag("batch", manager, reftagstr, this);
        }
    }

    /** Get batch reference tag which is an opaque id for the batch and is used in the client protocol.
     * Only running batches have a reference tag assigned.
     * @return Reference tag as a string, only valid if the batch is running.
     */
    const std::string& GetRefTagStr() const {
        return reftagstr;
    }

    /** Get batch type.
     * @return Batch type string.
     */
    const std::string& GetType() const {
        return type;
    }

    /** Check whether the batch is running.
     * Batches can be started with Manager::Start() and stopped with Manager::End().
     * @return True if the batch is running, false otherwise.
     */
    bool IsRunning() const {
        return (manager != NULL);
    }

    /** Get the batch start client protocol message.
     * The returned message object can be manipulated to add extra parameters or labels to the message. The first
     * parameter of the message is the batch reference tag generated by the module providing batch support.
     * If the batch type string was specified, it will be the second parameter of the message.
     * May only be called if IsRunning() == true.
     * @return Mutable batch start client protocol message.
     */
    ClientProtocol::Message& GetBatchStartMessage() {
        return *batchstartmsg;
    }

    /** Get the batch end client protocol message.
     * The returned message object can be manipulated to add extra parameters or labels to the message. The first
     * parameter of the message is the batch reference tag generated by the module providing batch support.
     * If the batch type string was specified, it will be the second parameter of the message.
     * May only be called if IsRunning() == true.
     * @return Mutable batch end client protocol message.
     */
    ClientProtocol::Message& GetBatchEndMessage() {
        return *batchendmsg;
    }

    friend class ManagerImpl;
};

/** Batch API. Use this to access the Manager.
 */
class IRCv3::Batch::API : public dynamic_reference_nocheck<Manager> {
  public:
    API(Module* mod)
        : dynamic_reference_nocheck<Manager>(mod, "batchapi") {
    }
};

/** Reference to the batch cap.
 * Can be used to check whether a user has the batch client cap enabled.
 */
class IRCv3::Batch::CapReference : public Cap::Reference {
  public:
    CapReference(Module* mod)
        : Cap::Reference(mod, "batch") {
    }
};
