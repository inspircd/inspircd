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

#ifndef SERIALIZE_H
#define SERIALIZE_H

#include <sstream>

#include "anope.h"
#include "base.h"

namespace Serialize {
class Data {
  public:
    enum Type {
        DT_TEXT,
        DT_INT
    };

    virtual ~Data() { }

    virtual std::iostream& operator[](const Anope::string &key) = 0;
    virtual std::set<Anope::string> KeySet() const {
        throw CoreException("Not supported");
    }
    virtual size_t Hash() const {
        throw CoreException("Not supported");
    }

    virtual void SetType(const Anope::string &key, Type t) { }
    virtual Type GetType(const Anope::string &key) const {
        return DT_TEXT;
    }
};

extern void RegisterTypes();
extern void CheckTypes();

class Type;
template<typename T> class Checker;
template<typename T> class Reference;
}

/** A serializable object. Serializable objects can be serialized into
 * abstract data types (Serialize::Data), and then reconstructed or
 * updated later at any time.
 */
class CoreExport Serializable : public virtual Base {
  private:
    /* A list of every serializable item in Anope.
     * Some of these are static and constructed at runtime,
     * so this list must be on the heap, as it is not always
     * constructed before other objects are if it isn't.
     */
    static std::list<Serializable *> *SerializableItems;
    friend class Serialize::Type;
    /* The type of item this object is */
    Serialize::Type *s_type;
    /* Iterator into serializable_items */
    std::list<Serializable *>::iterator s_iter;
    /* The hash of the last serialized form of this object committed to the database */
    size_t last_commit;
    /* The last time this object was committed to the database */
    time_t last_commit_time;

  protected:
    Serializable(const Anope::string &serialize_type);
    Serializable(const Serializable &);

    Serializable &operator=(const Serializable &);

  public:
    virtual ~Serializable();

    /* Unique ID (per type, not globally) for this object */
    uint64_t id;

    /* Only used by redis, to ignore updates */
    unsigned short redis_ignore;

    /** Marks the object as potentially being updated "soon".
     */
    void QueueUpdate();

    bool IsCached(Serialize::Data &);
    void UpdateCache(Serialize::Data &);

    bool IsTSCached();
    void UpdateTS();

    /** Get the type of serializable object this is
     * @return The serializable object type
     */
    Serialize::Type* GetSerializableType() const {
        return this->s_type;
    }

    virtual void Serialize(Serialize::Data &data) const = 0;

    static const std::list<Serializable *> &GetItems();
};

/* A serializable type. There should be one of these classes for each type
 * of class that inherits from Serializable. Used for unserializing objects
 * of this type, as it requires a function pointer to a static member function.
 */
class CoreExport Serialize::Type : public Base {
    typedef Serializable* (*unserialize_func)(Serializable *obj, Serialize::Data &);

    static std::vector<Anope::string> TypeOrder;
    static std::map<Anope::string, Serialize::Type *> Types;

    /* The name of this type, should be a class name */
    Anope::string name;
    unserialize_func unserialize;
    /* Owner of this type. Used for placing objects of this type in separate databases
     * based on what module, if any, owns it.
     */
    Module *owner;

    /* The timestamp for this type. All objects of this type are as up to date as
     * this timestamp. if curtime == timestamp then we have the most up to date
     * version of every object of this type.
     */
    time_t timestamp;

  public:
    /* Map of Serializable::id to Serializable objects */
    std::map<uint64_t, Serializable *> objects;

    /** Creates a new serializable type
     * @param n Type name
     * @param f Func to unserialize objects
     * @param owner Owner of this type. Leave NULL for the core.
     */
    Type(const Anope::string &n, unserialize_func f, Module *owner = NULL);
    ~Type();

    /** Gets the name for this type
     * @return The name, eg "NickAlias"
     */
    const Anope::string &GetName() {
        return this->name;
    }

    /** Unserialized an object.
     * @param obj NULL if this object doesn't yet exist. If this isn't NULL, instead
     * update the contents of this object.
     * @param data The data to unserialize
     * @return The unserialized object. If obj != NULL this should be obj.
     */
    Serializable *Unserialize(Serializable *obj, Serialize::Data &data);

    /** Check if this object type has any pending changes and update them.
     */
    void Check();

    /** Gets the timestamp for the object type. That is, the time we know
     * all objects of this type are updated at least to.
     */
    time_t GetTimestamp() const;

    /** Bumps object type timestamp to current time
     */
    void UpdateTimestamp();

    Module* GetOwner() const {
        return this->owner;
    }

    static Serialize::Type *Find(const Anope::string &name);

    static const std::vector<Anope::string> &GetTypeOrder();

    static const std::map<Anope::string, Serialize::Type *>& GetTypes();
};

/** Should be used to hold lists and other objects of a specific type,
 * but not a specific object. Used for ensuring that any access to
 * this object type is always up to date. These are usually constructed
 * at run time, before main is called, so no types are registered. This
 * is why there are static Serialize::Type* variables in every function.
 */
template<typename T>
class Serialize::Checker {
    Anope::string name;
    T obj;
    mutable ::Reference<Serialize::Type> type;

    inline void Check() const {
        if (!type) {
            type = Serialize::Type::Find(this->name);
        }
        if (type) {
            type->Check();
        }
    }

  public:
    Checker(const Anope::string &n) : name(n), type(NULL) { }

    inline const T* operator->() const {
        this->Check();
        return &this->obj;
    }
    inline T* operator->() {
        this->Check();
        return &this->obj;
    }

    inline const T& operator*() const {
        this->Check();
        return this->obj;
    }
    inline T& operator*() {
        this->Check();
        return this->obj;
    }

    inline operator const T&() const {
        this->Check();
        return this->obj;
    }
    inline operator T&() {
        this->Check();
        return this->obj;
    }
};

/** Used to hold references to serializable objects. Reference should always be
 * used when holding references to serializable objects for extended periods of time
 * to ensure that the object it refers to it always up to date. This also behaves like
 * Reference in that it will invalidate itself if the object it refers to is
 * destructed.
 */
template<typename T>
class Serialize::Reference : public ReferenceBase {
  protected:
    T *ref;

  public:
    Reference() : ref(NULL) {
    }

    Reference(T *obj) : ref(obj) {
        if (obj) {
            obj->AddReference(this);
        }
    }

    Reference(const Reference<T> &other) : ReferenceBase(other), ref(other.ref) {
        if (ref && !invalid) {
            this->ref->AddReference(this);
        }
    }

    ~Reference() {
        if (ref && !invalid) {
            this->ref->DelReference(this);
        }
    }

    inline Reference<T>& operator=(const Reference<T> &other) {
        if (this != &other) {
            if (ref && !invalid) {
                this->ref->DelReference(this);
            }

            this->ref = other.ref;
            this->invalid = other.invalid;

            if (ref && !invalid) {
                this->ref->AddReference(this);
            }
        }
        return *this;
    }

    inline operator bool() const {
        if (!this->invalid) {
            return this->ref != NULL;
        }
        return false;
    }

    inline operator T*() const {
        if (!this->invalid) {
            if (this->ref)
                // This can invalidate me
            {
                this->ref->QueueUpdate();
            }
            if (!this->invalid) {
                return this->ref;
            }
        }
        return NULL;
    }

    inline T* operator*() const {
        if (!this->invalid) {
            if (this->ref)
                // This can invalidate me
            {
                this->ref->QueueUpdate();
            }
            if (!this->invalid) {
                return this->ref;
            }
        }
        return NULL;
    }

    inline T* operator->() const {
        if (!this->invalid) {
            if (this->ref)
                // This can invalidate me
            {
                this->ref->QueueUpdate();
            }
            if (!this->invalid) {
                return this->ref;
            }
        }
        return NULL;
    }
};

#endif // SERIALIZE_H
