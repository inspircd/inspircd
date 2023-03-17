/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "services.h"
#include "anope.h"
#include "serialize.h"
#include "modules.h"
#include "account.h"
#include "bots.h"
#include "regchannel.h"
#include "xline.h"
#include "access.h"

using namespace Serialize;

std::vector<Anope::string> Type::TypeOrder;
std::map<Anope::string, Type *> Serialize::Type::Types;
std::list<Serializable *> *Serializable::SerializableItems;

void Serialize::RegisterTypes() {
    static Type nc("NickCore", NickCore::Unserialize), na("NickAlias",
            NickAlias::Unserialize), bi("BotInfo", BotInfo::Unserialize),
                      ci("ChannelInfo", ChannelInfo::Unserialize), access("ChanAccess",
                              ChanAccess::Unserialize),
                      akick("AutoKick", AutoKick::Unserialize), memo("Memo", Memo::Unserialize),
                      xline("XLine", XLine::Unserialize);
}

void Serialize::CheckTypes() {
    for (std::map<Anope::string, Serialize::Type *>::const_iterator it =
                Serialize::Type::GetTypes().begin(), it_end = Serialize::Type::GetTypes().end();
            it != it_end; ++it) {
        Serialize::Type *t = it->second;
        t->Check();
    }
}

Serializable::Serializable(const Anope::string &serialize_type) : last_commit(
        0), last_commit_time(0), id(0), redis_ignore(0) {
    if (SerializableItems == NULL) {
        SerializableItems = new std::list<Serializable *>();
    }
    SerializableItems->push_back(this);

    this->s_type = Type::Find(serialize_type);

    this->s_iter = SerializableItems->end();
    --this->s_iter;

    FOREACH_MOD(OnSerializableConstruct, (this));
}

Serializable::Serializable(const Serializable &other) : last_commit(0),
    last_commit_time(0), id(0), redis_ignore(0) {
    SerializableItems->push_back(this);
    this->s_iter = SerializableItems->end();
    --this->s_iter;

    this->s_type = other.s_type;

    FOREACH_MOD(OnSerializableConstruct, (this));
}

Serializable::~Serializable() {
    FOREACH_MOD(OnSerializableDestruct, (this));

    SerializableItems->erase(this->s_iter);
}

Serializable &Serializable::operator=(const Serializable &) {
    return *this;
}

void Serializable::QueueUpdate() {
    /* Schedule updater */
    FOREACH_MOD(OnSerializableUpdate, (this));

    /* Check for modifications now - this can delete this object! */
    FOREACH_MOD(OnSerializeCheck, (this->GetSerializableType()));
}

bool Serializable::IsCached(Serialize::Data &data) {
    return this->last_commit == data.Hash();
}

void Serializable::UpdateCache(Serialize::Data &data) {
    this->last_commit = data.Hash();
}

bool Serializable::IsTSCached() {
    return this->last_commit_time == Anope::CurTime;
}

void Serializable::UpdateTS() {
    this->last_commit_time = Anope::CurTime;
}

const std::list<Serializable *> &Serializable::GetItems() {
    return *SerializableItems;
}

Type::Type(const Anope::string &n, unserialize_func f, Module *o)  : name(n),
    unserialize(f), owner(o), timestamp(0) {
    TypeOrder.push_back(this->name);
    Types[this->name] = this;

    FOREACH_MOD(OnSerializeTypeCreate, (this));
}

Type::~Type() {
    /* null the type of existing serializable objects of this type */
    if (Serializable::SerializableItems != NULL)
        for (std::list<Serializable *>::iterator it =
                    Serializable::SerializableItems->begin();
                it != Serializable::SerializableItems->end(); ++it) {
            Serializable *s = *it;

            if (s->s_type == this) {
                s->s_type = NULL;
            }
        }

    std::vector<Anope::string>::iterator it = std::find(TypeOrder.begin(),
            TypeOrder.end(), this->name);
    if (it != TypeOrder.end()) {
        TypeOrder.erase(it);
    }
    Types.erase(this->name);
}

Serializable *Type::Unserialize(Serializable *obj, Serialize::Data &data) {
    return this->unserialize(obj, data);
}

void Type::Check() {
    FOREACH_MOD(OnSerializeCheck, (this));
}

time_t Type::GetTimestamp() const {
    return this->timestamp;
}

void Type::UpdateTimestamp() {
    this->timestamp = Anope::CurTime;
}

Type *Serialize::Type::Find(const Anope::string &name) {
    std::map<Anope::string, Type *>::iterator it = Types.find(name);
    if (it != Types.end()) {
        return it->second;
    }
    return NULL;
}

const std::vector<Anope::string> &Type::GetTypeOrder() {
    return TypeOrder;
}

const std::map<Anope::string, Serialize::Type *>& Type::GetTypes() {
    return Types;
}
