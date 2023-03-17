/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#ifndef EXTENSIBLE_H
#define EXTENSIBLE_H

#include "anope.h"
#include "serialize.h"
#include "service.h"
#include "logger.h"

class Extensible;

class CoreExport ExtensibleBase : public Service {
  protected:
    std::map<Extensible *, void *> items;

    ExtensibleBase(Module *m, const Anope::string &n);
    ~ExtensibleBase();

  public:
    virtual void Unset(Extensible *obj) = 0;

    /* called when an object we are keep track of is serializing */
    virtual void ExtensibleSerialize(const Extensible *, const Serializable *,
                                     Serialize::Data &) const { }
    virtual void ExtensibleUnserialize(Extensible *, Serializable *,
                                       Serialize::Data &) { }
};

class CoreExport Extensible {
  public:
    std::set<ExtensibleBase *> extension_items;

    virtual ~Extensible();

    void UnsetExtensibles();

    template<typename T> T* GetExt(const Anope::string &name) const;
    bool HasExt(const Anope::string &name) const;

    template<typename T> T* Extend(const Anope::string &name, const T &what);
    template<typename T> T* Extend(const Anope::string &name);
    template<typename T> T* Require(const Anope::string &name);
    template<typename T> void Shrink(const Anope::string &name);

    static void ExtensibleSerialize(const Extensible *, const Serializable *,
                                    Serialize::Data &data);
    static void ExtensibleUnserialize(Extensible *, Serializable *,
                                      Serialize::Data &data);
};

template<typename T>
class BaseExtensibleItem : public ExtensibleBase {
  protected:
    virtual T *Create(Extensible *) = 0;

  public:
    BaseExtensibleItem(Module *m, const Anope::string &n) : ExtensibleBase(m, n) { }

    ~BaseExtensibleItem() {
        while (!items.empty()) {
            std::map<Extensible *, void *>::iterator it = items.begin();
            Extensible *obj = it->first;
            T *value = static_cast<T *>(it->second);

            obj->extension_items.erase(this);
            items.erase(it);
            delete value;
        }
    }

    T* Set(Extensible *obj, const T &value) {
        T* t = Set(obj);
        if (t) {
            *t = value;
        }
        return t;
    }

    T* Set(Extensible *obj) {
        T* t = Create(obj);
        Unset(obj);
        items[obj] = t;
        obj->extension_items.insert(this);
        return t;
    }

    void Unset(Extensible *obj) anope_override {
        T *value = Get(obj);
        items.erase(obj);
        obj->extension_items.erase(this);
        delete value;
    }

    T* Get(const Extensible *obj) const {
        std::map<Extensible *, void *>::const_iterator it = items.find(
                    const_cast<Extensible *>(obj));
        if (it != items.end()) {
            return static_cast<T *>(it->second);
        }
        return NULL;
    }

    bool HasExt(const Extensible *obj) const {
        return items.find(const_cast<Extensible *>(obj)) != items.end();
    }

    T* Require(Extensible *obj) {
        T* t = Get(obj);
        if (t) {
            return t;
        }

        return Set(obj);
    }
};

template<typename T>
class ExtensibleItem : public BaseExtensibleItem<T> {
  protected:
    T* Create(Extensible *obj) anope_override {
        return new T(obj);
    }
  public:
    ExtensibleItem(Module *m, const Anope::string &n) : BaseExtensibleItem<T>(m,
                n) { }
};

template<typename T>
class PrimitiveExtensibleItem : public BaseExtensibleItem<T> {
  protected:
    T* Create(Extensible *obj) anope_override {
        return new T();
    }
  public:
    PrimitiveExtensibleItem(Module *m,
                            const Anope::string &n) : BaseExtensibleItem<T>(m, n) { }
};

template<>
class PrimitiveExtensibleItem<bool> : public BaseExtensibleItem<bool> {
  protected:
    bool* Create(Extensible *) anope_override {
        return NULL;
    }
  public:
    PrimitiveExtensibleItem(Module *m,
                            const Anope::string &n) : BaseExtensibleItem<bool>(m, n) { }
};

template<typename T>
class SerializableExtensibleItem : public PrimitiveExtensibleItem<T> {
  public:
    SerializableExtensibleItem(Module *m,
                               const Anope::string &n) : PrimitiveExtensibleItem<T>(m, n) { }

    void ExtensibleSerialize(const Extensible *e, const Serializable *s,
                             Serialize::Data &data) const anope_override {
        T* t = this->Get(e);
        data[this->name] << *t;
    }

    void ExtensibleUnserialize(Extensible *e, Serializable *s,
                               Serialize::Data &data) anope_override {
        T t;
        if (data[this->name] >> t) {
            this->Set(e, t);
        } else {
            this->Unset(e);
        }
    }
};

template<>
class SerializableExtensibleItem<bool> : public PrimitiveExtensibleItem<bool> {
  public:
    SerializableExtensibleItem(Module *m,
                               const Anope::string &n) : PrimitiveExtensibleItem<bool>(m, n) { }

    void ExtensibleSerialize(const Extensible *e, const Serializable *s,
                             Serialize::Data &data) const anope_override {
        data[this->name] << true;
    }

    void ExtensibleUnserialize(Extensible *e, Serializable *s,
                               Serialize::Data &data) anope_override {
        bool b = false;
        data[this->name] >> b;
        if (b) {
            this->Set(e);
        } else {
            this->Unset(e);
        }
    }
};

template<typename T>
struct ExtensibleRef : ServiceReference<BaseExtensibleItem<T> > {
    ExtensibleRef(const Anope::string &n) :
        ServiceReference<BaseExtensibleItem<T> >("Extensible", n) { }
};

template<typename T>
T* Extensible::GetExt(const Anope::string &name) const {
    ExtensibleRef<T> ref(name);
    if (ref) {
        return ref->Get(this);
    }

    Log(LOG_DEBUG) << "GetExt for nonexistent type " << name << " on " <<
                   static_cast<const void *>(this);
    return NULL;
}

template<typename T>
T* Extensible::Extend(const Anope::string &name, const T &what) {
    T* t = Extend<T>(name);
    if (t) {
        *t = what;
    }
    return t;
}

template<typename T>
T* Extensible::Extend(const Anope::string &name) {
    ExtensibleRef<T> ref(name);
    if (ref) {
        return ref->Set(this);
    }

    Log(LOG_DEBUG) << "Extend for nonexistent type " << name << " on " <<
                   static_cast<void *>(this);
    return NULL;
}

template<typename T>
T* Extensible::Require(const Anope::string &name) {
    if (HasExt(name)) {
        return GetExt<T>(name);
    } else {
        return Extend<T>(name);
    }
}

template<typename T>
void Extensible::Shrink(const Anope::string &name) {
    ExtensibleRef<T> ref(name);
    if (ref) {
        ref->Unset(this);
    } else {
        Log(LOG_DEBUG) << "Shrink for nonexistent type " << name << " on " <<
                       static_cast<void *>(this);
    }
}

#endif // EXTENSIBLE_H
