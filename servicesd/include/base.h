/*
 *
 * (C) 2008-2011 Adam <Adam@anope.org>
 * (C) 2008-2023 Anope Team <team@anope.org>
 *
 * Please read COPYING and README for further details.
 */

#ifndef BASE_H
#define BASE_H

#include "services.h"

/** The base class that most classes in Anope inherit from
 */
class CoreExport Base {
    /* References to this base class */
    std::set<ReferenceBase *> *references;
  public:
    Base();
    virtual ~Base();

    /** Adds a reference to this object. Eg, when a Reference
     * is created referring to this object this is called. It is used to
     * cleanup references when this object is destructed.
     */
    void AddReference(ReferenceBase *r);

    void DelReference(ReferenceBase *r);
};

class ReferenceBase {
  protected:
    bool invalid;
  public:
    ReferenceBase() : invalid(false) { }
    ReferenceBase(const ReferenceBase &other) : invalid(other.invalid) { }
    virtual ~ReferenceBase() { }
    inline void Invalidate() {
        this->invalid = true;
    }
};

/** Used to hold pointers to objects that may be deleted. A Reference will
 * no longer be valid once the object it refers is destructed.
 */
template<typename T>
class Reference : public ReferenceBase {
  protected:
    T *ref;
  public:
    Reference() : ref(NULL) {
    }

    Reference(T *obj) : ref(obj) {
        if (ref) {
            ref->AddReference(this);
        }
    }

    Reference(const Reference<T> &other) : ReferenceBase(other), ref(other.ref) {
        if (operator bool()) {
            ref->AddReference(this);
        }
    }

    virtual ~Reference() {
        if (operator bool()) {
            ref->DelReference(this);
        }
    }

    inline Reference<T>& operator=(const Reference<T> &other) {
        if (this != &other) {
            if (*this) {
                this->ref->DelReference(this);
            }

            this->ref = other.ref;
            this->invalid = other.invalid;

            if (*this) {
                this->ref->AddReference(this);
            }
        }
        return *this;
    }

    /* We explicitly call operator bool here in several places to prevent other
     * operators, such operator T*, from being called instead, which will mess
     * with any class inheriting from this that overloads this operator.
     */
    virtual operator bool() {
        if (!this->invalid) {
            return this->ref != NULL;
        }
        return false;
    }

    inline operator T*() {
        if (operator bool()) {
            return this->ref;
        }
        return NULL;
    }

    inline T* operator->() {
        if (operator bool()) {
            return this->ref;
        }
        return NULL;
    }

    inline T* operator*() {
        if (operator bool()) {
            return this->ref;
        }
        return NULL;
    }

    /** Note that we can't have an operator< that returns this->ref < other.ref
     * because this function is used to sort objects in containers (such as set
     * or map), and if the references themselves can change if the object they
     * refer to is invalidated or changed, then this screws with the order that
     * the objects would be in the container without properly adjusting the
     * container, resulting in weird stuff.
     *
     * As such, we don't allow storing references in containers that require
     * operator<, because they would not be able to compare what the references
     * actually referred to.
     */

    inline bool operator==(const Reference<T> &other) {
        if (!this->invalid) {
            return this->ref == other;
        }
        return false;
    }
};

#endif // BASE_H
