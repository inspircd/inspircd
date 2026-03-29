/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022-2024 Sadie Powell <sadie@witchery.services>
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

#include "stringutils.h"
#include "utility/string.h"

/** Base class for types that extend an extensible. */
class CoreExport ExtensionItem
	: public ServiceProvider
{
public:
	/** The type of extensible that this extension extends. */
	const ExtensionType extype:2;

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

	/** Called when a value for this extension is deleted.
	 * @param container The container that this extension is set on.
	 * @param item The value that is set on the container.
	 */
	virtual void OnDelete(const Extensible* container, const ExtensionPtr& item);

	/** Called when a value for this extension is synchronised across the network.
	 * @param container The container that this extension is set on.
	 * @param item The value that is set on the container.
	 * @param server The server which is being synchronised to or nullptr for a broadcast.
	 */
	virtual void OnSync(const Extensible* container, const ExtensionPtr& item, Server* server);

	/** @copydoc ServiceProvider::RegisterService */
	void RegisterService() override;

	/** Serialises a value for this extension of the specified container to the human-readable
	 *  format.
	 * @param container The container that this extension is set on.
	 * @param item The value to convert to the human-readable format.
	 * @return The value specified in \p item in the human-readable format.
	 */
	virtual std::string ToHuman(const Extensible* container, const ExtensionPtr& item) const noexcept;

	/** Serialises a value for this extension of the specified container to the internal format.
	 * @param container The container that this extension is set on.
	 * @param item The value to convert to the internal format.
	 * @return The value specified in \p item in the internal format.
	 */
	virtual std::string ToInternal(const Extensible* container, const ExtensionPtr& item) const noexcept;

	/** Serialises a value for this extension of the specified container to the network format.
	 * @param container The container that this extension is set on.
	 * @param item The value to convert to the network format.
	 * @return The value specified in \p item in the network format.
	 */
	virtual std::string ToNetwork(const Extensible* container, const ExtensionPtr& item) const noexcept;

protected:
	/** Initializes an instance of the ExtensionItem class.
	 * @param owner The module which created the extension.
	 * @param key The name of the extension (e.g. foo-bar).
	 * @param exttype The type of extensible that the extension applies to.
	 */
	ExtensionItem(const WeakModulePtr& owner, const std::string& key, ExtensionType exttype);

	/** Retrieves the value for this extension of the specified container from the internal map.
	 * @param container The container that this extension is set on.
	 * @return Either the value of this extension or nullptr if it does not exist.
	 */
	const ExtensionPtr* GetRaw(const Extensible* container) const;

	/** Sets a value for this extension of the specified container in the internal map and
	 *  returns the old value if one was set
	 * @param container The container that this extension should be set on.
	 * @param value The new value to set for this extension. Will NOT be copied.
	 * @return Either the old value or nullptr if one is not set.
	 */
	ExtensionPtr SetRaw(Extensible* container, const ExtensionPtr& value);

	/** Syncs the value of this extension of the specified container across the network. Does
	 *   nothing if an inheritor does not implement ExtensionItem::ToNetwork.
	 * @param container The container that this extension is set on.
	 * @param item The value of this extension.
	 */
	void Sync(const Extensible* container, const ExtensionPtr& item);

	/** Removes this extension from the specified container and returns it.
	 * @param container The container that this extension should be removed from.
	 * @return Either the old value of this extension or nullptr if it was not set.
	 */
	ExtensionPtr UnsetRaw(Extensible* container);
};

/** An extension which has a simple (usually POD) value. */
template <typename Value, typename Del = std::default_delete<Value>>
class SimpleExtItem
	: public ExtensionItem
{
protected:
	/** Whether to sync this extension across the network. */
	bool synced;

public:
	/** The underlying pointer type. */
	using ValuePtr = std::shared_ptr<Value>;

	/** Initializes an instance of the SimpleExtItem<T,Del> class.
	 * @param owner The module which created the extension.
	 * @param key The name of the extension (e.g. foo-bar).
	 * @param exttype The type of extensible that the extension applies to.
	 * @param sync Whether this extension should be broadcast to other servers.
	 */
	SimpleExtItem(const WeakModulePtr& owner, const std::string& key, ExtensionType exttype, bool sync = false)
		: ExtensionItem(owner, key, exttype)
		, synced(sync)
	{
	}

	/** @copydoc ExtensionItem::FromNetwork */
	void FromNetwork(Extensible* container, const std::string& value) noexcept override
	{
		if (synced)
			FromInternal(container, value);
	}

	/** @copydoc ExtensionItem::ToNetwork */
	std::string ToNetwork(const Extensible* container, const ExtensionPtr& item) const noexcept override
	{
		return synced ? ToInternal(container, item) : std::string();
	}

	/** Creates a new shared pointer with the deleter specified in the extension item.
	 * @param args The arguments to forward to the constructor of \p T.
	 */
	template <typename... Args>
	ValuePtr Create(Args&&... args)
	{
		auto* ptr = new Value(std::forward<Args>(args)...);
		return ValuePtr(ptr, Del());
	}

	/** Retrieves the value for this extension of the specified container.
	 * @param container The container that this extension is set on.
	 * @return Either a pointer to the value of this extension or nullptr if it is not set.
	 */
	inline Value* Get(const Extensible* container) const
	{
		auto* ptr = GetRaw(container);
		return ptr ? std::static_pointer_cast<Value>(*ptr).get() : nullptr;
	}

	/** Retrieves the value for this extension of the specified container.
	 * @param container The container that this extension is set on.
	 * @return A shared pointer to the value of this extension, empty if it is not set.
	 */
	inline ValuePtr GetPtr(const Extensible* container) const
	{
		auto* ptr = GetRaw(container);
		return ptr ? ValuePtr() : std::static_pointer_cast<Value>(*ptr);
	}

	/** Retrieves the value for this extension of the specified container.
	 * @param container The container that this extension is set on.
	 * @return A reference to the value of this extension.
	 */
	inline Value& GetRef(Extensible* container)
	{
		auto* ptr = GetRaw(container);
		if (ptr)
			return *std::static_pointer_cast<Value>(*ptr).get();

		auto value = Create();
		Set(container, value, false);
		return *value.get();
	}

	/** Sets a value for this extension of the specified container.
	 * @param container The container that this extension should be set on.
	 * @param value The new value to set for this extension. Will NOT be copied.
	 * @param sync If syncable then whether to sync this set to the network.
	 */
	inline void Set(Extensible* container, const ValuePtr& value, bool sync = true)
	{
		if (container->extype != this->extype)
			return;

		auto old = std::static_pointer_cast<Value>(SetRaw(container, value));
		OnDelete(container, old);
		if (sync && synced)
			Sync(container, value);
	}

	/** Sets a value for this extension of the specified container.
	 * @param container The container that this extension should be set on.
	 * @param value The new value to set for this extension. Will NOT be copied.
	 * @param sync If syncable then whether to sync this set to the network.
	 */
	inline void Set(Extensible* container, Value* value, bool sync = true)
	{
		if (container->extype == this->extype)
			Set(container, ValuePtr(value,  Del()), sync);
	}

	/** Sets a value for this extension of the specified container.
	 * @param container The container that this extension should be set on.
	 * @param value The new value to set for this extension. Will be copied.
	 * @param sync If syncable then whether to sync this set to the network.
	 */
	inline void Set(Extensible* container, const Value& value, bool sync = true)
	{
		if (container->extype == this->extype)
			Set(container, Create(value), sync);
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
		if (container->extype == this->extype)
			Set(container, Create(std::forward<Args>(args)...), false);
	}

	/** Removes this extension from the specified container.
	 * @param container The container that this extension should be removed from.
	 * @param sync If syncable then whether to sync this unset to the network.
	 */
	inline void Unset(Extensible* container, bool sync = true)
	{
		if (container->extype != this->extype)
			return;

		OnDelete(container, UnsetRaw(container));
		if (synced && sync)
			Sync(container, nullptr);
	}
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
	BoolExtItem(const WeakModulePtr& owner, const std::string& key, ExtensionType exttype, bool sync = false);

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
	std::string ToHuman(const Extensible* container, const ExtensionPtr& item) const noexcept override;

	/** @copydoc ExtensionItem::ToInternal */
	std::string ToInternal(const Extensible* container, const ExtensionPtr& item) const noexcept override;

	/** @copydoc ExtensionItem::ToNetwork */
	std::string ToNetwork(const Extensible* container, const ExtensionPtr& item) const noexcept override;

	/** Removes this extension from the specified container.
	 * @param container The container that this extension should be removed from.
	 * @param sync If syncable then whether to sync this unset to the network.
	 */
	void Unset(Extensible* container, bool sync = true);
};

/** An extension which has a list value. */
template <typename Container, typename Del = std::default_delete<Container>>
class ListExtItem
	: public SimpleExtItem<Container, Del>
{
public:
	/** The underlying list type. */
	using List = Container;

	/** A pointer to the underlying list type. */
	using ListPtr = std::shared_ptr<List>;

	/** Initializes an instance of the ListExtItem class.
	 * @param owner The module which created the extension.
	 * @param key The name of the extension (e.g. foo-bar).
	 * @param exttype The type of extensible that the extension applies to.
	 * @param sync Whether this extension should be broadcast to other servers.
	 */
	ListExtItem(const WeakModulePtr& owner, const std::string& key, ExtensionType exttype, bool sync = false)
		: SimpleExtItem<List>(owner, key, exttype, sync)
	{
	}

	/** @copydoc ExtensionItem::FromInternal */
	void FromInternal(Extensible* container, const std::string& value) noexcept override
	{
		if (container->extype != this->extype)
			return;

		if (value.empty())
		{
			SimpleExtItem<Container>::Unset(container, false);
			return;
		}

		ListPtr list;
		StringSplitter stream(value);
		for (std::string element; stream.GetToken(element); )
		{
			if (!list)
				list = this->Create();

			// Argh! Why doesn't vector<string> have an insert(value_type) method?
			if constexpr (std::is_same_v<Container, std::vector<typename Container::value_type>>)
				list->push_back(Percent::Decode(element));
			else
				list->insert(Percent::Decode(element));
		}

		if (!list)
		{
			// The remote sent an empty list.
			SimpleExtItem<Container>::Unset(container, false);
		}
		else
		{
			// The remote sent a non-zero list.
			SimpleExtItem<Container>::Set(container, list, false);
		}
	}

	/** @copydoc ExtensionItem::ToInternal */
	std::string ToHuman(const Extensible* container, const ExtensionPtr& item) const noexcept override
	{
		const auto& list = std::static_pointer_cast<List>(item);
		if (!list || list->empty())
			return {};

		return insp::join(*list, ' ');
	}

	/** @copydoc ExtensionItem::ToInternal */
	std::string ToInternal(const Extensible* container, const ExtensionPtr& item) const noexcept override
	{
		const auto& list = std::static_pointer_cast<List>(item);
		if (!list || list->empty())
			return {};

		std::string value;
		for (const auto& element : *list)
			value.append(Percent::Encode(element)).push_back(' ');
		value.pop_back();

		return value;
	}
};

/** An extension which has a raw integer value. */
class CoreExport IntExtItem
	: public ExtensionItem
{
protected:
	/** Whether to sync this extension across the network. */
	bool synced;

	/** Initializes an instance of the IntExtItem class.
	 * @param owner The module which created the extension.
	 * @param key The name of the extension (e.g. foo-bar).
	 * @param exttype The type of extensible that the extension applies to.
	 * @param sync Whether this extension should be broadcast to other servers.
	 */
	IntExtItem(const WeakModulePtr& owner, const std::string& key, ExtensionType exttype, bool sync);

public:
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
	std::string ToInternal(const Extensible* container, const ExtensionPtr& item) const noexcept override;

	/** @copydoc ExtensionItem::ToNetwork */
	std::string ToNetwork(const Extensible* container, const ExtensionPtr& item) const noexcept override;

	/** Removes this extension from the specified container.
	 * @param container The container that this extension should be removed from.
	 * @param sync If syncable then whether to sync this unset to the network.
	 */
	void Unset(Extensible* container, bool sync = true);
};

/** An extension which has a typed integer value. */
template <typename Value>
class NumExtItem
	: public IntExtItem
{
public:
	/** The numeric type for this extension. */
	using Numeric = std::conditional_t<std::is_enum_v<Value>, std::underlying_type<Value>, std::type_identity<Value>>::type;

	/** Initializes an instance of the NumExtItem class.
	 * @param owner The module which created the extension.
	 * @param key The name of the extension (e.g. foo-bar).
	 * @param exttype The type of extensible that the extension applies to.
	 * @param sync Whether this extension should be broadcast to other servers.
	 */
	NumExtItem(const WeakModulePtr& owner, const std::string& key, ExtensionType exttype, bool sync = false)
		: IntExtItem(owner, key, exttype, sync)
	{
	}

	/** @copydoc ExtensionItem::FromInternal */
	void FromInternal(Extensible* container, const std::string& value) noexcept override
	{
		Set(container, ConvToNum<Numeric>(value), false);
	}

	/** @copydoc IntExtItem::Get */
	Numeric Get(const Extensible* container) const
	{
		return static_cast<Numeric>(IntExtItem::Get(container));
	}

	/** @copydoc IntExtItem::Set */
	void Set(Extensible* container, Numeric value, bool sync = true)
	{
		IntExtItem::Set(container, static_cast<intptr_t>(value), sync);
	}

	/** @copydoc ExtensionItem::ToInternal */
	std::string ToInternal(const Extensible* container, const ExtensionPtr& item) const noexcept override
	{
		return ConvToStr(static_cast<Numeric>(reinterpret_cast<intptr_t>(item.get())));
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
	StringExtItem(const WeakModulePtr& owner, const std::string& key, ExtensionType exttype, bool sync = false);

	/** @copydoc ExtensionItem::FromInternal */
	void FromInternal(Extensible* container, const std::string& value) noexcept override;

	/** @copydoc ExtensionItem::ToInternal */
	std::string ToInternal(const Extensible* container, const ExtensionPtr& item) const noexcept override;
};
