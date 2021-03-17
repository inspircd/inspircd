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

/** Base class for logic that extends an Extensible object. */
class CoreExport ExtensionItem
	: public ServiceProvider
	, public usecountbase
{
 public:
	/** Types of Extensible that an ExtensionItem can apply to. */
	enum ExtensibleType
	{
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
	 * @param owner The module which created this ExtensionItem.
	 * @param key The name of the extension item (e.g. ssl_cert).
	 * @param exttype The type of Extensible that this ExtensionItem applies to.
	 */
	ExtensionItem(Module* owner, const std::string& key, ExtensibleType exttype);

	/** Destroys an instance of the ExtensionItem class. */
	virtual ~ExtensionItem() = default;

	/** Sets an ExtensionItem using a value in the internal format.
	 * @param container A container the ExtensionItem should be set on.
	 * @param value A value in the internal format.
	 */
	virtual void FromInternal(Extensible* container, const std::string& value) noexcept;

	/** Sets an ExtensionItem using a value in the network format.
	 * @param container A container the ExtensionItem should be set on.
	 * @param value A value in the network format.
	 */
	virtual void FromNetwork(Extensible* container, const std::string& value) noexcept;

	/** Gets an ExtensionItem's value in a human-readable format.
	 * @param container The container the ExtensionItem is set on.
	 * @param item The value to convert to a human-readable format.
	 * @return The value specified in \p item in a human readable format.
	 */
	virtual std::string ToHuman(const Extensible* container, void* item) const noexcept;

	/** Gets an ExtensionItem's value in the internal format.
	 * @param container The container the ExtensionItem is set on.
	 * @param item The value to convert to the internal format.
	 * @return The value specified in \p item in the internal format.
	 */
	virtual std::string ToInternal(const Extensible* container, void* item) const noexcept;

	/** Gets an ExtensionItem's value in the network format.
	 * @param container The container the ExtensionItem is set on.
	 * @param item The value to convert to the network format.
	 * @return The value specified in \p item in the network format.
	 */
	virtual std::string ToNetwork(const Extensible* container, void* item) const noexcept;

	/** Deallocates the specified ExtensionItem value.
	 * @param container The container that the ExtensionItem is set on.
	 * @param item The item to deallocate.
	 */
	virtual void Delete(Extensible* container, void* item) = 0;

	/** Registers this object with the ExtensionManager. */
	void RegisterService() override;

 protected:
	/** Retrieves the value for this ExtensionItem from the internal map.
	 * @param container The container that the ExtensionItem is set on.
	 * @return Either the value of this ExtensionItem or NULL if it is not set.
	 */
	void* GetRaw(const Extensible* container) const;

	/** Stores a value for this ExtensionItem in the internal map and returns the old value if one was set.
	 * @param container A container the ExtensionItem should be set on.
	 * @param value The value to set on the specified container.
	 * @return Either the old value or NULL if one is not set.
	 */
	void* SetRaw(Extensible* container, void* value);

	/** Removes the value for this ExtensionItem from the internal map and returns it.
	 * @param container A container the ExtensionItem should be removed from.
	 * @return Either the old value or NULL if one is not set.
	*/
	void* UnsetRaw(Extensible* container);

	/** Syncs the value of this ExtensionItem across the network.
	 * @param container The container this ExtensionItem is set on.
	 * @param item The value of this ExtensionItem.
	 */
	void Sync(const Extensible* container, void* item);
};

/** class Extensible is the parent class of many classes such as User and Channel.
 * class Extensible implements a system which allows modules to 'extend' the class by attaching data within
 * a map associated with the object. In this way modules can store their own custom information within user
 * objects, channel objects and server objects, without breaking other modules (this is more sensible than using
 * a flags variable, and each module defining bits within the flag as 'theirs' as it is less prone to conflict and
 * supports arbitrary data storage).
 */
class CoreExport Extensible
	: public Cullable
	, public Serializable
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
	Cullable::Result Cull() override;
	virtual ~Extensible();
	void UnhookExtensions(const std::vector<reference<ExtensionItem>>& toRemove);

	/**
	 * Free all extension items attached to this Extensible
	 */
	void FreeAllExtItems();

	/** @copydoc Serializable::Deserialize */
	bool Deserialize(Data& data) override;

	/** @copydoc Serializable::Deserialize */
	bool Serialize(Serializable::Data& data) override;
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

/** Represents a simple ExtensionItem. */
template <typename T, typename Del = std::default_delete<T>>
class SimpleExtItem : public ExtensionItem
{
 public:
	/** Initializes an instance of the SimpleExtItem class.
	 * @param parent The module which created this SimpleExtItem.
	 * @param Key The name of the extension item (e.g. ssl_cert).
	 * @param exttype The type of Extensible that this SimpleExtItem applies to.
	 */
	SimpleExtItem(Module* parent, const std::string& Key, ExtensibleType exttype)
		: ExtensionItem(parent, Key, exttype)
	{
	}

	/** Destroys an instance of the SimpleExtItem class. */
	virtual ~SimpleExtItem() = default;

	inline T* Get(const Extensible* container) const
	{
		return static_cast<T*>(GetRaw(container));
	}

	inline void Set(Extensible* container, T* value, bool sync = true)
	{
		T* old = static_cast<T*>(SetRaw(container, value));
		Delete(container, old);
		if (sync)
			Sync(container, value);
	}

	template <typename... Args>
	inline void Set(Extensible* container, Args&&... args)
	{
		Set(container, new T(std::forward<Args>(args)...));
	}

	inline void Unset(Extensible* container, bool sync = true)
	{
		Delete(container, UnsetRaw(container));
		if (sync)
			Sync(container, nullptr);
	}

	void Delete(Extensible* container, void* item) override
	{
		Del del;
		del(static_cast<T*>(item));
	}
};

/** Encapsulates an ExtensionItem which has a string value. */
class CoreExport StringExtItem : public SimpleExtItem<std::string>
{
 protected:
	/** Whether to sync this StringExtItem across the network. */
	bool synced;

 public:
	/** Initializes an instance of the StringExtItem class.
	 * @param owner The module which created this StringExtItem.
	 * @param key The name of the extension item (e.g. ssl_cert).
	 * @param exttype The type of Extensible that this IntExtItem applies to.
	 * @param sync Whether this StringExtItem should be broadcast to other servers.
	 */
	StringExtItem(Module* owner, const std::string& key, ExtensibleType exttype, bool sync = false);

	/** Destroys an instance of the StringExtItem class. */
	virtual ~StringExtItem() = default;

	/** @copydoc ExtensionItem::FromInternal */
	void FromInternal(Extensible* container, const std::string& value) noexcept override;

	/** @copydoc ExtensionItem::FromNetwork */
	void FromNetwork(Extensible* container, const std::string& value) noexcept override;

	/** @copydoc ExtensionItem::ToInternal */
	std::string ToInternal(const Extensible* container, void* item) const noexcept override;

	/** @copydoc ExtensionItem::ToNetwork */
	std::string ToNetwork(const Extensible* container, void* item) const noexcept override;
};

/** Encapsulates an ExtensionItem which has a integer value. */
class CoreExport IntExtItem : public ExtensionItem
{
 protected:
	/** Whether to sync this IntExtItem across the network. */
	bool synced;

 public:
	/** Initializes an instance of the IntExtItem class.
	 * @param owner The module which created this IntExtItem.
	 * @param key The name of the extension item (e.g. ssl_cert).
	 * @param exttype The type of Extensible that this IntExtItem applies to.
	 * @param sync Whether this IntExtItem should be broadcast to other servers.
	 */
	IntExtItem(Module* owner, const std::string& key, ExtensibleType exttype, bool sync = false);

	/** Destroys an instance of the IntExtItem class. */
	virtual ~IntExtItem() = default;

	/** @copydoc ExtensionItem::Delete */
	void Delete(Extensible* container, void* item) override;

	/** Retrieves the value for this IntExtItem.
	 * @param container The container that the IntExtItem is set on.
	 * @return Either the value of this IntExtItem or NULL if it is not set.
	 */
	intptr_t Get(const Extensible* container) const;

	/** @copydoc ExtensionItem::FromInternal */
	void FromInternal(Extensible* container, const std::string& value) noexcept override;

	/** @copydoc ExtensionItem::FromNetwork */
	void FromNetwork(Extensible* container, const std::string& value) noexcept override;

	/** Sets a value for this IntExtItem.
	 * @param container A container that the IntExtItem should be set on.
	 * @param value The new value for this IntExtItem.
	 * @param sync Whether to sync this value to other servers.
	 */
	void Set(Extensible* container, intptr_t value, bool sync = true);

	/** @copydoc ExtensionItem::ToInternal */
	std::string ToInternal(const Extensible* container, void* item) const noexcept override;

	/** @copydoc ExtensionItem::ToNetwork */
	std::string ToNetwork(const Extensible* container, void* item) const noexcept override;

	/** Removes the value for this IntExtItem.
	 * @param container A container the ExtensionItem should be removed from.
	 * @param sync Whether to sync this unset to the network.
	 */
	void Unset(Extensible* container, bool sync = true);
};

/** Encapsulates an ExtensionItem which has a boolean value. */
class CoreExport BoolExtItem : public ExtensionItem
{
 protected:
	/** Whether to sync this BoolExtItem across the network. */
	bool synced;

 public:
	/** Initializes an instance of the BoolExtItem class.
	 * @param owner The module which created this BoolExtItem.
	 * @param key The name of the extension item (e.g. ssl_cert).
	 * @param exttype The type of Extensible that this BoolExtItem applies to.
	 * @param sync Whether this BoolExtItem should be broadcast to other servers.
	 */
	BoolExtItem(Module* owner, const std::string& key, ExtensibleType exttype, bool sync = false);

	/** Destroys an instance of the BoolExtItem class. */
	virtual ~BoolExtItem() = default;

	/** @copydoc ExtensionItem::Delete */
	void Delete(Extensible* container, void* item) override;

	/** @copydoc ExtensionItem::FromInternal */
	void FromInternal(Extensible* container, const std::string& value) noexcept override;

	/** @copydoc ExtensionItem::FromNetwork */
	void FromNetwork(Extensible* container, const std::string& value) noexcept override;

	/** @copydoc ExtensionItem::ToHuman */
	std::string ToHuman(const Extensible* container, void* item) const noexcept override;

	/** @copydoc ExtensionItem::ToInternal */
	std::string ToInternal(const Extensible* container, void* item) const noexcept override;

	/** @copydoc ExtensionItem::ToNetwork */
	std::string ToNetwork(const Extensible* container, void* item) const noexcept override;

	/** Retrieves the value for this BoolExtItem.
	 * @param container The container that the BoolExtItem is set on.
	 * @return Either the value of this BoolExtItem or NULL if it is not set.
	 */
	bool Get(const Extensible* container) const;

	/** Sets a value for this BoolExtItem.
	 * @param container A container that the BoolExtItem should be set on.
	 * @param sync Whether to sync this set to the network.
	 */
	void Set(Extensible* container, bool sync = true);

	/** Removes the value for this BoolExtItem.
	 * @param container A container the ExtensionItem should be removed from.
	 * @param sync Whether to sync this unset to the network.
	 */
	void Unset(Extensible* container, bool sync = true);
};
