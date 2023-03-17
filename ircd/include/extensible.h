/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

/** DEPRECATED: use {To,From}{Human,Internal,Network} instead. */
enum SerializeFormat {
    FORMAT_USER,
    FORMAT_INTERNAL,
    FORMAT_NETWORK,
    FORMAT_PERSIST
};

/** Base class for logic that extends an Extensible object. */
class CoreExport ExtensionItem : public ServiceProvider, public usecountbase {
  public:
    /** Types of Extensible that an ExtensionItem can apply to. */
    enum ExtensibleType {
        /** The ExtensionItem applies to a User object. */
        EXT_USER,

        /** The ExtensionItem applies to a Channel object. */
        EXT_CHANNEL,

        /** The ExtensionItem applies to a Membership object. */
        EXT_MEMBERSHIP
    };

    /** The type of Extensible that this ExtensionItem applies to. */
    const ExtensibleType type;

    /** Initializes an instance of the ExtensionItem class.
     * @param key The name of the extension item (e.g. ssl_cert).
     * @param exttype The type of Extensible that this ExtensionItem applies to.
     * @param owner The module which created this ExtensionItem.
     */
    ExtensionItem(const std::string& key, ExtensibleType exttype, Module* owner);

    /** Destroys an instance of the ExtensionItem class. */
    virtual ~ExtensionItem();

    /** Sets an ExtensionItem using a value in the internal format.
     * @param container A container the ExtensionItem should be set on.
     * @param value A value in the internal format.
     */
    virtual void FromInternal(Extensible* container, const std::string& value);

    /** Sets an ExtensionItem using a value in the network format.
     * @param container A container the ExtensionItem should be set on.
     * @param value A value in the network format.
     */
    virtual void FromNetwork(Extensible* container, const std::string& value);

    /** Gets an ExtensionItem's value in a human-readable format.
     * @param container The container the ExtensionItem is set on.
     * @param item The value to convert to a human-readable format.
     * @return The value specified in \p item in a human readable format.
     */
    virtual std::string ToHuman(const Extensible* container, void* item) const;
    /** Gets an ExtensionItem's value in the internal format.
     * @param container The container the ExtensionItem is set on.
     * @param item The value to convert to the internal format.
     * @return The value specified in \p item in the internal format.
     */
    virtual std::string ToInternal(const Extensible* container, void* item) const ;

    /** Gets an ExtensionItem's value in the network format.
     * @param container The container the ExtensionItem is set on.
     * @param item The value to convert to the network format.
     * @return The value specified in \p item in the network format.
     */
    virtual std::string ToNetwork(const Extensible* container, void* item) const;

    /** Deallocates the specified ExtensionItem value.
     * @param container The container that the ExtensionItem is set on.
     * @param item The item to deallocate.
     */
    virtual void free(Extensible* container, void* item) = 0;

    /** Registers this object with the ExtensionManager. */
    void RegisterService() CXX11_OVERRIDE;

    /** DEPRECATED: use To{Human,Internal,Network} instead. */
    DEPRECATED_METHOD(virtual std::string serialize(SerializeFormat format,
                      const Extensible* container, void* item) const);

    /** DEPRECATED: use From{Internal,Network} instead. */
    DEPRECATED_METHOD(virtual void unserialize(SerializeFormat format,
                      Extensible* container, const std::string& value));

  protected:
    /** Retrieves the value for this ExtensionItem from the internal map.
     * @param container The container that the ExtensionItem is set on.
     * @return Either the value of this ExtensionItem or NULL if it is not set.
     */
    void* get_raw(const Extensible* container) const;

    /** Stores a value for this ExtensionItem in the internal map and returns the old value if one was set.
     * @param container A container the ExtensionItem should be set on.
     * @param value The value to set on the specified container.
     * @return Either the old value or NULL if one is not set.
     */
    void* set_raw(Extensible* container, void* value);

    /** Removes the value for this ExtensionItem from the internal map and returns it.
     * @param container A container the ExtensionItem should be removed from.
     * @return Either the old value or NULL if one is not set.
    */
    void* unset_raw(Extensible* container);
};

/** class Extensible is the parent class of many classes such as User and Channel.
 * class Extensible implements a system which allows modules to 'extend' the class by attaching data within
 * a map associated with the object. In this way modules can store their own custom information within user
 * objects, channel objects and server objects, without breaking other modules (this is more sensible than using
 * a flags variable, and each module defining bits within the flag as 'theirs' as it is less prone to conflict and
 * supports arbitrary data storage).
 */
class CoreExport Extensible
    : public classbase
    , public Serializable {
  public:
    typedef insp::flat_map<reference<ExtensionItem>, void*> ExtensibleStore;

    // Friend access for the protected getter/setter
    friend class ExtensionItem;
  private:
    /** Private data store.
     * Holds all extensible metadata for the class.
     */
    ExtensibleStore extensions;

    /** True if this Extensible has been culled.
     * A warning is generated if false on destruction.
     */
    unsigned int culled:1;
  public:
    /**
     * Get the extension items for iteration (i.e. for metadata sync during netburst)
     */
    inline const ExtensibleStore& GetExtList() const {
        return extensions;
    }

    Extensible();
    CullResult cull() CXX11_OVERRIDE;
    virtual ~Extensible();
    void doUnhookExtensions(const std::vector<reference<ExtensionItem> >& toRemove);

    /**
     * Free all extension items attached to this Extensible
     */
    void FreeAllExtItems();

    /** @copydoc Serializable::Deserialize */
    bool Deserialize(Data& data) CXX11_OVERRIDE;

    /** @copydoc Serializable::Deserialize */
    bool Serialize(Serializable::Data& data) CXX11_OVERRIDE;
};

class CoreExport ExtensionManager {
  public:
    typedef std::map<std::string, reference<ExtensionItem> > ExtMap;

    bool Register(ExtensionItem* item);
    void BeginUnregister(Module* module,
                         std::vector<reference<ExtensionItem> >& list);
    ExtensionItem* GetItem(const std::string& name);

    /** Get all registered extensions keyed by their names
     * @return Const map of ExtensionItem pointers keyed by their names
     */
    const ExtMap& GetExts() const {
        return types;
    }

  private:
    ExtMap types;
};

/** DEPRECATED: use ExtensionItem instead. */
typedef ExtensionItem LocalExtItem;

template <typename T, typename Del = stdalgo::defaultdeleter<T> >
class SimpleExtItem : public ExtensionItem {
  public:
    SimpleExtItem(const std::string& Key, ExtensibleType exttype, Module* parent)
        : ExtensionItem(Key, exttype, parent) {
    }

    virtual ~SimpleExtItem() {
    }

    inline T* get(const Extensible* container) const {
        return static_cast<T*>(get_raw(container));
    }

    inline void set(Extensible* container, const T& value) {
        T* ptr = new T(value);
        T* old = static_cast<T*>(set_raw(container, ptr));
        free(container, old);
    }

    inline void set(Extensible* container, T* value) {
        T* old = static_cast<T*>(set_raw(container, value));
        free(container, old);
    }

    inline void unset(Extensible* container) {
        T* old = static_cast<T*>(unset_raw(container));
        free(container, old);
    }

    void free(Extensible* container, void* item) CXX11_OVERRIDE {
        Del del;
        del(static_cast<T*>(item));
    }
};

class CoreExport LocalStringExt : public SimpleExtItem<std::string> {
  public:
    LocalStringExt(const std::string& key, ExtensibleType exttype, Module* owner);
    virtual ~LocalStringExt();
    std::string ToInternal(const Extensible* container,
                           void* item) const CXX11_OVERRIDE;
    void FromInternal(Extensible* container,
                      const std::string& value) CXX11_OVERRIDE;
};

class CoreExport LocalIntExt : public ExtensionItem {
  public:
    LocalIntExt(const std::string& key, ExtensibleType exttype, Module* owner);
    virtual ~LocalIntExt();
    std::string ToInternal(const Extensible* container,
                           void* item) const CXX11_OVERRIDE;
    void FromInternal(Extensible* container,
                      const std::string& value) CXX11_OVERRIDE;
    intptr_t get(const Extensible* container) const;
    intptr_t set(Extensible* container, intptr_t value);
    void unset(Extensible* container) {
        set(container, 0);
    }
    void free(Extensible* container, void* item) CXX11_OVERRIDE;
};

class CoreExport StringExtItem : public ExtensionItem {
  public:
    StringExtItem(const std::string& key, ExtensibleType exttype, Module* owner);
    virtual ~StringExtItem();
    std::string* get(const Extensible* container) const;
    std::string ToNetwork(const Extensible* container,
                          void* item) const CXX11_OVERRIDE;
    void FromNetwork(Extensible* container,
                     const std::string& value) CXX11_OVERRIDE;
    void set(Extensible* container, const std::string& value);
    void unset(Extensible* container);
    void free(Extensible* container, void* item) CXX11_OVERRIDE;
};
