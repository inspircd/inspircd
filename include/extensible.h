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

/** Types of extensible that an extension can extend. */
enum class ExtensionType
	: uint8_t
{
	/** The extension extends the User class. */
	USER = 0,

	/** The extension extends the Channel class. */
	CHANNEL = 1,

	/** The extension extends the Membership class. */
	MEMBERSHIP = 2,
};

/** Base class for types that extend an extensible. */
class CoreExport ExtensionItem
	: public ServiceProvider
{
public:
	/** The type of extensible that this extension extends. */
	const ExtensionType type;

	/** Deletes a \p value which is set on \p container.
	 * @param container The container that this extension is set on.
	 * @param item The item to delete.
	 */
	virtual void Delete(Extensible* container, void* item) = 0;

	/** Deserialises a value for this extension of the specified container from the internal format.
	 * @param container A container this extension should be set on.
	 * @param value A value in the internal format.
	 */
	virtual void FromInternal(Extensible* container, const std::string& value) noexcept;

	/** Deserialises a value for this extension of the specified container from the network format.
	 * @param container A container this extension should be set on.
	 * @param value A value in the network format.
	 */
	virtual void FromNetwork(Extensible* container, const std::string& value) noexcept;

	/** @copydoc ServiceProvider::RegisterService */
	void RegisterService() override;

	/** Serialises a value for this extension of the specified container to the human-readable
	 *  format.
	 * @param container The container that this extension is set on.
	 * @param item The value to convert to the human-readable format.
	 * @return The value specified in \p item in the human-readable format.
	 */
	virtual std::string ToHuman(const Extensible* container, void* item) const noexcept;

	/** Serialises a value for this extension of the specified container to the internal format.
	 * @param container The container that this extension is set on.
	 * @param item The value to convert to the internal format.
	 * @return The value specified in \p item in the internal format.
	 */
	virtual std::string ToInternal(const Extensible* container, void* item) const noexcept;

	/** Serialises a value for this extension of the specified container to the network format.
	 * @param container The container that this extension is set on.
	 * @param item The value to convert to the network format.
	 * @return The value specified in \p item in the network format.
	 */
	virtual std::string ToNetwork(const Extensible* container, void* item) const noexcept;

protected:
	/** Initializes an instance of the ExtensionItem class.
	 * @param owner The module which created the extension.
	 * @param key The name of the extension (e.g. foo-bar).
	 * @param exttype The type of extensible that the extension applies to.
	 */
	ExtensionItem(Module* owner, const std::string& key, ExtensionType exttype);

	/** Retrieves the value for this extension of the specified container from the internal map.
	 * @param container The container that this extension is set on.
	 * @return Either the value of this extension or nullptr if it does not exist.
	 */
	void* GetRaw(const Extensible* container) const;

	/** Sets a value for this extension of the specified container in the internal map and
	 *  returns the old value if one was set
	 * @param container The container that this extension should be set on.
	 * @param value The new value to set for this extension. Will NOT be copied.
	 * @return Either the old value or nullptr if one is not set.
	 */
	void* SetRaw(Extensible* container, void* value);

	/** Syncs the value of this extension of the specified container across the network. Does
	 *   nothing if an inheritor does not implement ExtensionItem::ToNetwork.
	 * @param container The container that this extension is set on.
	 * @param item The value of this extension.
	 */
	void Sync(const Extensible* container, void* item);

	/** Removes this extension from the specified container and returns it.
	 * @param container The container that this extension should be removed from.
	 * @return Either the old value of this extension or nullptr if it was not set.
	 */
	void* UnsetRaw(Extensible* container);
};

/** Base class for types which can be extended with additional data. */
class CoreExport Extensible
	: public Cullable
	, public Serializable
{
public:
	/** The container which extension values are stored in. */
	typedef insp::flat_map<ExtensionItem*, void*> ExtensibleStore;

	/** Allows extensions to access the extension store. */
	friend class ExtensionItem;

	~Extensible() override;

	/** @copydoc Cullable::Cull */
	Cullable::Result Cull() override;

	/** @copydoc Serializable::Deserialize */
	bool Deserialize(Data& data) override;

	/** Frees all extensions attached to this extensible. */
	void FreeAllExtItems();

	/** Retrieves the values for extensions which are set on this extensible. */
	const ExtensibleStore& GetExtList() const { return extensions; }

	/** @copydoc Serializable::Deserialize */
	bool Serialize(Serializable::Data& data) override;

	/** Unhooks the specifies extensions from this extensible.
	 * @param items The items to unhook.
	 */
	void UnhookExtensions(const std::vector<ExtensionItem*>& items);

protected:
	Extensible();

private:
	/** The values for extensions which are set on this extensible. */
	ExtensibleStore extensions;

	/** Whether this extensible has been culled yet. */
	bool culled:1;
};

/** Manager for the extension system */
class CoreExport ExtensionManager final
{
public:
	/** The container which registered extensions are stored in. */
	typedef std::map<std::string, ExtensionItem*> ExtMap;

	/** Begins unregistering extensions belonging to the specified module.
	 * @param module The module to unregister extensions for.
	 * @param list The list to add unregistered extensions to.
	 */
	void BeginUnregister(Module* module, std::vector<ExtensionItem*>& list);

	/** Retrieves registered extensions keyed by their names. */
	const ExtMap& GetExts() const { return types; }

	/** Retrieves an extension by name.
	 * @param name The name of the extension to retrieve.
	 * @return Either the value of this extension or nullptr if it does not exist.
	 */
	ExtensionItem* GetItem(const std::string& name);

	/** Registers an extension with the manager.
	 * @return Either true if the extension was registered or false if an extension with the same
	 *         name already exists.
	 */
	bool Register(ExtensionItem* item);

private:
	/** Registered extensions keyed by their names. */
	ExtMap types;
};

/** An extension which has a simple (usually POD) value. */
template <typename T, typename Del = std::default_delete<T>>
class SimpleExtItem
	: public ExtensionItem
{
protected:
	/** Whether to sync this extension across the network. */
	bool synced;

public:
	/** Initializes an instance of the SimpleExtItem<T,Del> class.
	 * @param owner The module which created the extension.
	 * @param key The name of the extension (e.g. foo-bar).
	 * @param exttype The type of extensible that the extension applies to.
	 * @param sync Whether this extension should be broadcast to other servers.
	 */
	SimpleExtItem(Module* owner, const std::string& key, ExtensionType exttype, bool sync = false)
		: ExtensionItem(owner, key, exttype)
		, synced(sync)
	{
	}

	/** @copydoc ExtensionItem::Delete */
	void Delete(Extensible* container, void* item) override
	{
		Del del;
		del(static_cast<T*>(item));
	}

	/** Retrieves the value for this extension of the specified container.
	 * @param container The container that this extension is set on.
	 * @return Either the value of this extension or nullptr if it is not set.
	 */
	inline T* Get(const Extensible* container) const
	{
		return static_cast<T*>(GetRaw(container));
	}

	/** Sets a value for this extension of the specified container.
	 * @param container The container that this extension should be set on.
	 * @param value The new value to set for this extension. Will NOT be copied.
	 * @param sync If syncable then whether to sync this set to the network.
	 */
	inline void Set(Extensible* container, T* value, bool sync = true)
	{
		T* old = static_cast<T*>(SetRaw(container, value));
		Delete(container, old);
		if (sync && synced)
			Sync(container, value);
	}

	/** Sets a value for this extension of the specified container.
	 * @param container The container that this extension should be set on.
	 * @param value The new value to set for this extension. Will be copied.
	 * @param sync If syncable then whether to sync this set to the network.
	 */
	inline void Set(Extensible* container, const T& value, bool sync = true)
	{
		Set(container, new T(value), sync);
	}

	/** Sets a forwarded value for this extension of the specified container.
	 * @param container The container that this extension should be set on.
	 * @param args The arguments to forward to the constructor of \p T.
	 */
	template <typename... Args>
	inline void SetFwd(Extensible* container, Args&&... args)
	{
		// Forwarded arguments are for complex types which are assumed to not
		// be synced across the network. You can manually call Sync() if this
		// is not the case.
		Set(container, new T(std::forward<Args>(args)...), false);
	}

	/** Removes this extension from the specified container.
	 * @param container The container that this extension should be removed from.
	 * @param sync If syncable then whether to sync this unset to the network.
	 */
	inline void Unset(Extensible* container, bool sync = true)
	{
		Delete(container, UnsetRaw(container));
		if (synced && sync)
			Sync(container, nullptr);
	}
};

/** An extension which has a string value. */
class CoreExport StringExtItem
	: public SimpleExtItem<std::string>
{
public:
	/** Initializes an instance of the StringExtItem class.
	 * @param owner The module which created the extension.
	 * @param key The name of the extension (e.g. foo-bar).
	 * @param exttype The type of extensible that the extension applies to.
	 * @param sync Whether this extension should be broadcast to other servers.
	 */
	StringExtItem(Module* owner, const std::string& key, ExtensionType exttype, bool sync = false);

	/** @copydoc ExtensionItem::FromInternal */
	void FromInternal(Extensible* container, const std::string& value) noexcept override;

	/** @copydoc ExtensionItem::FromNetwork */
	void FromNetwork(Extensible* container, const std::string& value) noexcept override;

	/** @copydoc ExtensionItem::ToInternal */
	std::string ToInternal(const Extensible* container, void* item) const noexcept override;

	/** @copydoc ExtensionItem::ToNetwork */
	std::string ToNetwork(const Extensible* container, void* item) const noexcept override;
};

/** An extension which has an integer value. */
class CoreExport IntExtItem
	: public ExtensionItem
{
protected:
	/** Whether to sync this extension across the network. */
	bool synced;

public:
	/** Initializes an instance of the IntExtItem class.
	 * @param owner The module which created the extension.
	 * @param key The name of the extension (e.g. foo-bar).
	 * @param exttype The type of extensible that the extension applies to.
	 * @param sync Whether this extension should be broadcast to other servers.
	 */
	IntExtItem(Module* owner, const std::string& key, ExtensionType exttype, bool sync = false);

	/** @copydoc ExtensionItem::Delete */
	void Delete(Extensible* container, void* item) override;

	/** Retrieves the value for this extension of the specified container.
	 * @param container The container that this extension is set on.
	 * @return Either the value of this extension or 0 if it is not set.
	 */
	intptr_t Get(const Extensible* container) const;

	/** @copydoc ExtensionItem::FromInternal */
	void FromInternal(Extensible* container, const std::string& value) noexcept override;

	/** @copydoc ExtensionItem::FromNetwork */
	void FromNetwork(Extensible* container, const std::string& value) noexcept override;

	/** Sets a value for this extension of the specified container.
	 * @param container The container that this extension should be set on.
	 * @param value The new value to set for this extension.
	 * @param sync If syncable then whether to sync this set to the network.
	 */
	void Set(Extensible* container, intptr_t value, bool sync = true);

	/** @copydoc ExtensionItem::ToInternal */
	std::string ToInternal(const Extensible* container, void* item) const noexcept override;

	/** @copydoc ExtensionItem::ToNetwork */
	std::string ToNetwork(const Extensible* container, void* item) const noexcept override;

	/** Removes this extension from the specified container.
	 * @param container The container that this extension should be removed from.
	 * @param sync If syncable then whether to sync this unset to the network.
	 */
	void Unset(Extensible* container, bool sync = true);
};

/** An extension which has a boolean value. */
class CoreExport BoolExtItem
	: public ExtensionItem
{
protected:
	/** Whether to sync this extension across the network. */
	bool synced;

public:
	/** Initializes an instance of the BoolExtItem class.
	 * @param owner The module which created the extension.
	 * @param key The name of the extension (e.g. foo-bar).
	 * @param exttype The type of extensible that the extension applies to.
	 * @param sync Whether this extension should be broadcast to other servers.
	 */
	BoolExtItem(Module* owner, const std::string& key, ExtensionType exttype, bool sync = false);

	/** @copydoc ExtensionItem::Delete */
	void Delete(Extensible* container, void* item) override;

	/** Retrieves the value for this extension of the specified container.
	 * @param container The container that this extension is set on.
	 * @return Either the value of this extension or false if it is not set.
	 */
	bool Get(const Extensible* container) const;

	/** @copydoc ExtensionItem::FromInternal */
	void FromInternal(Extensible* container, const std::string& value) noexcept override;

	/** @copydoc ExtensionItem::FromNetwork */
	void FromNetwork(Extensible* container, const std::string& value) noexcept override;

	/** Sets a value for this extension of the specified container.
	 * @param container The container that this extension should be set on.
	 * @param sync If syncable then whether to sync this set to the network.
	 */
	void Set(Extensible* container, bool sync = true);

	/** @copydoc ExtensionItem::ToHuman */
	std::string ToHuman(const Extensible* container, void* item) const noexcept override;

	/** @copydoc ExtensionItem::ToInternal */
	std::string ToInternal(const Extensible* container, void* item) const noexcept override;

	/** @copydoc ExtensionItem::ToNetwork */
	std::string ToNetwork(const Extensible* container, void* item) const noexcept override;

	/** Removes this extension from the specified container.
	 * @param container The container that this extension should be removed from.
	 * @param sync If syncable then whether to sync this unset to the network.
	 */
	void Unset(Extensible* container, bool sync = true);
};
