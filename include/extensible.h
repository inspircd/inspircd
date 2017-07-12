/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

enum SerializeFormat
{
	/** Shown to a human (does not need to be unserializable) */
	FORMAT_USER,
	/** Passed internally to this process (i.e. for /RELOADMODULE) */
	FORMAT_INTERNAL,
	/** Passed to other servers on the network (i.e. METADATA s2s command) */
	FORMAT_NETWORK,
	/** Stored on disk (i.e. permchannel database) */
	FORMAT_PERSIST
};

/** Class represnting an extension of some object
 */
class CoreExport ExtensionItem : public ServiceProvider, public usecountbase
{
 public:
	/** Extensible subclasses
	 */
	enum ExtensibleType
	{
		EXT_USER,
		EXT_CHANNEL,
		EXT_MEMBERSHIP
	};

	/** Type (subclass) of Extensible that this ExtensionItem is valid for
	 */
	const ExtensibleType type;

	ExtensionItem(const std::string& key, ExtensibleType exttype, Module* owner);
	virtual ~ExtensionItem();
	/** Serialize this item into a string
	 *
	 * @param format The format to serialize to
	 * @param container The object containing this item
	 * @param item The item itself
	 */
	virtual std::string serialize(SerializeFormat format, const Extensible* container, void* item) const = 0;
	/** Convert the string form back into an item
	 * @param format The format to serialize from (not FORMAT_USER)
	 * @param container The object that this item applies to
	 * @param value The return from a serialize() call that was run elsewhere with this key
	 */
	virtual void unserialize(SerializeFormat format, Extensible* container, const std::string& value) = 0;
	/** Free the item */
	virtual void free(void* item) = 0;

	/** Register this object in the ExtensionManager
	 */
	void RegisterService() CXX11_OVERRIDE;

 protected:
	/** Get the item from the internal map */
	void* get_raw(const Extensible* container) const;
	/** Set the item in the internal map; returns old value */
	void* set_raw(Extensible* container, void* value);
	/** Remove the item from the internal map; returns old value */
	void* unset_raw(Extensible* container);
};

/** class Extensible is the parent class of many classes such as User and Channel.
 * class Extensible implements a system which allows modules to 'extend' the class by attaching data within
 * a map associated with the object. In this way modules can store their own custom information within user
 * objects, channel objects and server objects, without breaking other modules (this is more sensible than using
 * a flags variable, and each module defining bits within the flag as 'theirs' as it is less prone to conflict and
 * supports arbitary data storage).
 */
class CoreExport Extensible : public classbase
{
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
	 * Get the extension items for iteraton (i.e. for metadata sync during netburst)
	 */
	inline const ExtensibleStore& GetExtList() const { return extensions; }

	Extensible();
	virtual CullResult cull() CXX11_OVERRIDE;
	virtual ~Extensible();
	void doUnhookExtensions(const std::vector<reference<ExtensionItem> >& toRemove);

	/**
	 * Free all extension items attached to this Extensible
	 */
	void FreeAllExtItems();
};

class CoreExport ExtensionManager
{
 public:
	typedef std::map<std::string, reference<ExtensionItem> > ExtMap;

	bool Register(ExtensionItem* item);
	void BeginUnregister(Module* module, std::vector<reference<ExtensionItem> >& list);
	ExtensionItem* GetItem(const std::string& name);

	/** Get all registered extensions keyed by their names
	 * @return Const map of ExtensionItem pointers keyed by their names
	 */
	const ExtMap& GetExts() const { return types; }

 private:
	ExtMap types;
};

/** Base class for items that are NOT synchronized between servers */
class CoreExport LocalExtItem : public ExtensionItem
{
 public:
	LocalExtItem(const std::string& key, ExtensibleType exttype, Module* owner);
	virtual ~LocalExtItem();
	virtual std::string serialize(SerializeFormat format, const Extensible* container, void* item) const;
	virtual void unserialize(SerializeFormat format, Extensible* container, const std::string& value);
	virtual void free(void* item) = 0;
};

template <typename T, typename Del = stdalgo::defaultdeleter<T> >
class SimpleExtItem : public LocalExtItem
{
 public:
	SimpleExtItem(const std::string& Key, ExtensibleType exttype, Module* parent)
		: LocalExtItem(Key, exttype, parent)
	{
	}

	virtual ~SimpleExtItem()
	{
	}

	inline T* get(const Extensible* container) const
	{
		return static_cast<T*>(get_raw(container));
	}

	inline void set(Extensible* container, const T& value)
	{
		T* ptr = new T(value);
		T* old = static_cast<T*>(set_raw(container, ptr));
		Del del;
		del(old);
	}

	inline void set(Extensible* container, T* value)
	{
		T* old = static_cast<T*>(set_raw(container, value));
		Del del;
		del(old);
	}

	inline void unset(Extensible* container)
	{
		T* old = static_cast<T*>(unset_raw(container));
		Del del;
		del(old);
	}

	virtual void free(void* item)
	{
		Del del;
		del(static_cast<T*>(item));
	}
};

class CoreExport LocalStringExt : public SimpleExtItem<std::string>
{
 public:
	LocalStringExt(const std::string& key, ExtensibleType exttype, Module* owner);
	virtual ~LocalStringExt();
	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const;
	void unserialize(SerializeFormat format, Extensible* container, const std::string& value);
};

class CoreExport LocalIntExt : public LocalExtItem
{
 public:
	LocalIntExt(const std::string& key, ExtensibleType exttype, Module* owner);
	virtual ~LocalIntExt();
	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const;
	void unserialize(SerializeFormat format, Extensible* container, const std::string& value);
	intptr_t get(const Extensible* container) const;
	intptr_t set(Extensible* container, intptr_t value);
	void unset(Extensible* container) { set(container, 0); }
	void free(void* item);
};

class CoreExport StringExtItem : public ExtensionItem
{
 public:
	StringExtItem(const std::string& key, ExtensibleType exttype, Module* owner);
	virtual ~StringExtItem();
	std::string* get(const Extensible* container) const;
	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const;
	void unserialize(SerializeFormat format, Extensible* container, const std::string& value);
	void set(Extensible* container, const std::string& value);
	void unset(Extensible* container);
	void free(void* item);
};
