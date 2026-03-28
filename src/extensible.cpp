/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2024 Sadie Powell <sadie@witchery.services>
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


#include "inspircd.h"
#include "extension.h"

namespace
{
	// These templates are used by BoolExtItem and IntExtItem to allow storing a
	// value within the pointer address of a shared pointer.
	template <typename T>
	ExtensionPtr CreateFakePointer(T value)
	{
		return ExtensionPtr(reinterpret_cast<void*>(value), [](auto*) { });
	}

	template <typename T>
	T GetFakePointer(const ExtensionPtr* ptr)
	{
		return ptr ? reinterpret_cast<T>(ptr->get()) : T();
	}
}

bool ExtensionManager::Register(ExtensionItem* item)
{
	return types.emplace(item->service_name, item).second;
}

void ExtensionManager::BeginUnregister(const ModulePtr& module, std::vector<ExtensionItem*>& items)
{
	for (ExtMap::iterator type = types.begin(); type != types.end(); )
	{
		ExtMap::iterator thistype = type++;
		ExtensionItem* item = thistype->second;

		if (insp::same_ptr(item->service_creator, module))
		{
			items.push_back(item);
			types.erase(thistype);
		}
	}
}

ExtensionItem* ExtensionManager::GetItem(const std::string& name)
{
	ExtMap::iterator iter = types.find(name);
	if (iter == types.end())
		return nullptr;

	return iter->second;
}

Extensible::Extensible(ExtensionType exttype)
	: extype(exttype)
	, culled(false)
{
}

Extensible::~Extensible()
{
	if ((!extensions.empty() || !culled) && ServerInstance)
	{
		ServerInstance->Logs.Debug("CULL", "Extensible was deleted without being culled: @{}",
			(void*)this);
	}
}

Cullable::Result Extensible::Cull()
{
	FreeAllExtItems();
	culled = true;
	return Cullable::Cull();
}

void Extensible::FreeAllExtItems()
{
	for (const auto& [extension, item] : extensions)
		extension->OnDelete(this, item);
	extensions.clear();
}

void Extensible::UnhookExtensions(const std::vector<ExtensionItem*>& items)
{
	for (auto* item : items)
	{
		ExtensibleStore::iterator iter = extensions.find(item);
		if (iter != extensions.end())
		{
			item->OnDelete(this, iter->second);
			extensions.erase(iter);
		}
	}
}

ExtensionItem::ExtensionItem(const WeakModulePtr& mod, const std::string& Key, ExtensionType exttype)
	: ServiceProvider(mod, "ExtensionItem", Key)
	, extype(exttype)
{
}

void ExtensionItem::OnDelete(const Extensible* container, const ExtensionPtr& item)
{
}

void ExtensionItem::OnSync(const Extensible* container, const ExtensionPtr& item, Server* server)
{
}

void ExtensionItem::RegisterService()
{
	if (!ServerInstance->Extensions.Register(this))
		throw ModuleException(this->service_creator, "Extension already exists: {}", this->service_name);
}

const ExtensionPtr* ExtensionItem::GetRaw(const Extensible* container) const
{
	auto iter = container->extensions.find(const_cast<ExtensionItem*>(this));
	if (iter == container->extensions.end())
		return nullptr;

	return &iter->second;
}

ExtensionPtr ExtensionItem::SetRaw(Extensible* container, const ExtensionPtr& value)
{
	auto result = container->extensions.emplace(this, value);
	if (result.second)
		return nullptr;

	auto old = result.first->second;
	result.first->second = value;
	return old;
}

ExtensionPtr ExtensionItem::UnsetRaw(Extensible* container)
{
	auto iter = container->extensions.find(this);
	if (iter == container->extensions.end())
		return nullptr;

	auto result = iter->second;
	container->extensions.erase(iter);
	return result;
}

void ExtensionItem::Sync(const Extensible* container, const ExtensionPtr& item)
{
	const std::string networkstr = item ? ToNetwork(container, item) : "";
	ServerInstance->PI->SendMetadata(container, this->service_name, networkstr);
	OnSync(container, item, nullptr);
}

void ExtensionItem::FromInternal(Extensible* container, const std::string& value) noexcept
{
}

void ExtensionItem::FromNetwork(Extensible* container, const std::string& value) noexcept
{
}

std::string ExtensionItem::ToHuman(const Extensible* container, const ExtensionPtr& item) const noexcept
{
	// Try to use the network form by default.
	std::string ret = ToNetwork(container, item);

	// If there's no network form then fall back to the internal form.
	if (ret.empty())
		ret = ToInternal(container, item);

	return ret;
}

std::string ExtensionItem::ToInternal(const Extensible* container, const ExtensionPtr& item) const noexcept
{
	return {};
}

std::string ExtensionItem::ToNetwork(const Extensible* container, const ExtensionPtr& item) const noexcept
{
	return {};
}

BoolExtItem::BoolExtItem(const WeakModulePtr& owner, const std::string& key, ExtensionType exttype, bool sync)
	: ExtensionItem(owner, key, exttype)
	, synced(sync)
{
}

void BoolExtItem::FromInternal(Extensible* container, const std::string& value) noexcept
{
	if (ConvToNum<intptr_t>(value))
		Set(container, false);
	else
		Unset(container, false);
}

std::string BoolExtItem::ToHuman(const Extensible* container, const ExtensionPtr& item) const noexcept
{
	return item ? "set" : "unset";
}

void BoolExtItem::FromNetwork(Extensible* container, const std::string& value) noexcept
{
	if (synced)
		FromInternal(container, value);
}

std::string BoolExtItem::ToInternal(const Extensible* container, const ExtensionPtr& item) const noexcept
{
	return ConvToStr(!!item);
}

std::string BoolExtItem::ToNetwork(const Extensible* container, const ExtensionPtr& item) const noexcept
{
	return synced ? ToInternal(container, item) : std::string();
}

bool BoolExtItem::Get(const Extensible* container) const
{
	return GetRaw(container);
}

void BoolExtItem::Set(Extensible* container, bool sync)
{
	if (container->extype != this->extype)
		return;

	auto ptr = CreateFakePointer(1);
	SetRaw(container, ptr);
	if (sync && synced)
		Sync(container, ptr);
}

void BoolExtItem::Unset(Extensible* container, bool sync)
{
	if (container->extype != this->extype)
		return;

	UnsetRaw(container);
	if (sync && synced)
		Sync(container, CreateFakePointer(0));
}

IntExtItem::IntExtItem(const WeakModulePtr& owner, const std::string& key, ExtensionType exttype, bool sync)
	: ExtensionItem(owner, key, exttype)
	, synced(sync)
{
}

void IntExtItem::FromInternal(Extensible* container, const std::string& value) noexcept
{
	Set(container, ConvToNum<intptr_t>(value), false);
}

void IntExtItem::FromNetwork(Extensible* container, const std::string& value) noexcept
{
	if (synced)
		FromInternal(container, value);
}

intptr_t IntExtItem::Get(const Extensible* container) const
{
	return GetFakePointer<intptr_t>(GetRaw(container));
}

void IntExtItem::Set(Extensible* container, intptr_t value, bool sync)
{
	if (container->extype != this->extype)
		return;

	if (value)
		SetRaw(container, CreateFakePointer(value));
	else
		UnsetRaw(container);

	if (sync && synced)
	{
		auto* ptr = GetRaw(container);
		Sync(container, ptr ? *ptr : nullptr);
	}
}

std::string IntExtItem::ToInternal(const Extensible* container, const ExtensionPtr& item) const noexcept
{
	return ConvToStr(GetFakePointer<intptr_t>(&item));
}

std::string IntExtItem::ToNetwork(const Extensible* container, const ExtensionPtr& item) const noexcept
{
	return synced ? ToInternal(container, item) : std::string();
}

void IntExtItem::Unset(Extensible* container, bool sync)
{
	if (container->extype != this->extype)
		return;

	UnsetRaw(container);
	if (sync && synced)
		Sync(container, nullptr);
}

StringExtItem::StringExtItem(const WeakModulePtr& owner, const std::string& key, ExtensionType exttype, bool sync)
	: SimpleExtItem(owner, key, exttype, sync)
{
}

void StringExtItem::FromInternal(Extensible* container, const std::string& value) noexcept
{
	if (value.empty())
		Unset(container, false);
	else
		Set(container, value, false);
}


std::string StringExtItem::ToInternal(const Extensible* container, const ExtensionPtr& item) const noexcept
{
	return item ? *std::static_pointer_cast<std::string>(item) : std::string();
}

