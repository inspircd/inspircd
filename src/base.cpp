/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd_config.h"
#include "base.h"
#include <time.h>
#include "inspircd.h"
#include <typeinfo>

classbase::classbase()
{
	if (ServerInstance && ServerInstance->Logs)
		ServerInstance->Logs->Log("CULLLIST", DEBUG, "classbase::+%s @%p",
			typeid(*this).name(), (void*)this);
}

CullResult classbase::cull()
{
	if (ServerInstance && ServerInstance->Logs)
		ServerInstance->Logs->Log("CULLLIST", DEBUG, "classbase::-%s @%p",
			typeid(*this).name(), (void*)this);
	return CullResult();
}

classbase::~classbase()
{
	if (ServerInstance && ServerInstance->Logs)
		ServerInstance->Logs->Log("CULLLIST", DEBUG, "classbase::~%s @%p",
			typeid(*this).name(), (void*)this);
}

CullResult::CullResult()
{
}

refcountbase::refcountbase() : refcount(0)
{
}

refcountbase::~refcountbase()
{
}

ExtensionItem::ExtensionItem(const std::string& Key, Module* mod) : key(Key), owner(mod)
{
}

ExtensionItem::~ExtensionItem()
{
}

void* ExtensionItem::get_raw(const Extensible* container) const
{
	Extensible::ExtensibleStore::const_iterator i =
		container->extensions.find(const_cast<ExtensionItem*>(this));
	if (i == container->extensions.end())
		return NULL;
	return i->second;
}

void* ExtensionItem::set_raw(Extensible* container, void* value)
{
	std::pair<Extensible::ExtensibleStore::iterator,bool> rv =
		container->extensions.insert(std::make_pair(this, value));
	if (rv.second)
	{
		return NULL;
	}
	else
	{
		void* old = rv.first->second;
		rv.first->second = value;
		return old;
	}
}

void* ExtensionItem::unset_raw(Extensible* container)
{
	Extensible::ExtensibleStore::iterator i = container->extensions.find(this);
	if (i == container->extensions.end())
		return NULL;
	void* rv = i->second;
	container->extensions.erase(i);
	return rv;
}

void ExtensionManager::Register(ExtensionItem* item)
{
	types.insert(std::make_pair(item->key, item));
}

void ExtensionManager::BeginUnregister(Module* module, std::vector<ExtensionItem*>& list)
{
	std::map<std::string, ExtensionItem*>::iterator i = types.begin();
	while (i != types.end())
	{
		std::map<std::string, ExtensionItem*>::iterator me = i++;
		ExtensionItem* item = me->second;
		if (item->owner == module)
		{
			list.push_back(item);
			types.erase(me);
		}
	}
}

ExtensionItem* ExtensionManager::GetItem(const std::string& name)
{
	std::map<std::string, ExtensionItem*>::iterator i = types.find(name);
	if (i == types.end())
		return NULL;
	return i->second;
}

void Extensible::doUnhookExtensions(const std::vector<ExtensionItem*>& toRemove)
{
	for(std::vector<ExtensionItem*>::const_iterator i = toRemove.begin(); i != toRemove.end(); ++i)
	{
		ExtensionItem* item = *i;
		ExtensibleStore::iterator e = extensions.find(item);
		if (e != extensions.end())
		{
			item->free(e->second);
			extensions.erase(e);
		}
	}
}

CullResult Extensible::cull()
{
	for(ExtensibleStore::iterator i = extensions.begin(); i != extensions.end(); ++i)
	{
		i->first->free(i->second);	
	}
	return classbase::cull();
}

Extensible::~Extensible()
{
}

LocalExtItem::LocalExtItem(const std::string& Key, Module* mod) : ExtensionItem(Key, mod)
{
}

LocalExtItem::~LocalExtItem()
{
}

std::string LocalExtItem::serialize(SerializeFormat format, const Extensible* container, void* item) const
{
	return "";
}

void LocalExtItem::unserialize(SerializeFormat format, Extensible* container, const std::string& value)
{
}

LocalStringExt::LocalStringExt(const std::string& Key, Module* Owner)
	: SimpleExtItem<std::string>(Key, Owner) { }

LocalStringExt::~LocalStringExt()
{
}

std::string LocalStringExt::serialize(SerializeFormat format, const Extensible* container, void* item) const
{
	if (item && format == FORMAT_USER)
		return *static_cast<std::string*>(item);
	return "";
}

LocalIntExt::LocalIntExt(const std::string& Key, Module* mod) : LocalExtItem(Key, mod)
{
}

LocalIntExt::~LocalIntExt()
{
}

std::string LocalIntExt::serialize(SerializeFormat format, const Extensible* container, void* item) const
{
	if (format != FORMAT_USER)
		return "";
	return ConvToStr(reinterpret_cast<intptr_t>(item));
}

intptr_t LocalIntExt::get(const Extensible* container) const
{
	return reinterpret_cast<intptr_t>(get_raw(container));
}

intptr_t LocalIntExt::set(Extensible* container, intptr_t value)
{
	if (value)
		return reinterpret_cast<intptr_t>(set_raw(container, reinterpret_cast<void*>(value)));
	else
		return reinterpret_cast<intptr_t>(unset_raw(container));
}

void LocalIntExt::free(void*)
{
}

StringExtItem::StringExtItem(const std::string& Key, Module* mod) : ExtensionItem(Key, mod)
{
}

StringExtItem::~StringExtItem()
{
}

std::string* StringExtItem::get(const Extensible* container) const
{
	return static_cast<std::string*>(get_raw(container));
}

std::string StringExtItem::serialize(SerializeFormat format, const Extensible* container, void* item) const
{
	return item ? *static_cast<std::string*>(item) : "";
}

void StringExtItem::unserialize(SerializeFormat format, Extensible* container, const std::string& value)
{
	if (value.empty())
		unset(container);
	else
		set(container, value);
}

void StringExtItem::set(Extensible* container, const std::string& value)
{
	void* old = set_raw(container, new std::string(value));
	delete static_cast<std::string*>(old);
}

void StringExtItem::unset(Extensible* container)
{
	void* old = unset_raw(container);
	delete static_cast<std::string*>(old);
}

void StringExtItem::free(void* item)
{
	delete static_cast<std::string*>(item);
}
