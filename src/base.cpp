/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <typeinfo>

#ifdef VT_DEBUG
std::set<void*>* alloc_list = NULL;
static void alloc_list_add_(void* item)
{
	if (!alloc_list)
		alloc_list = new std::set<classbase*>;
	alloc_list->insert(item);
}
#define alloc_list_add(x) alloc_list_add_(static_cast<void*>(x))
#define alloc_list_del(x) alloc_list->erase(x)
#else
#define alloc_list_add(x) do { } while (0)
#define alloc_list_del(x) do { } while (0)
#endif

classbase::classbase()
{
	if (ServerInstance && ServerInstance->Logs)
		ServerInstance->Logs->Log("CULLLIST", DEBUG, "classbase::+ @%p", (void*)this);
	alloc_list_add(this);
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
		ServerInstance->Logs->Log("CULLLIST", DEBUG, "classbase::~ @%p", (void*)this);
	alloc_list_del(this);
}

CullResult::CullResult()
{
}

// This trick detects heap allocations of refcountbase objects
static void* last_heap = NULL;

void* refcountbase::operator new(size_t size)
{
	last_heap = ::operator new(size);
	return last_heap;
}

void refcountbase::operator delete(void* obj)
{
	if (last_heap == obj)
		last_heap = NULL;
	::operator delete(obj);
}

refcountbase::refcountbase() : refcount(0)
{
	if (this != last_heap)
		throw CoreException("Reference allocate on the stack!");
	alloc_list_add(this);
}

refcountbase::~refcountbase()
{
	if (refcount && ServerInstance && ServerInstance->Logs)
		ServerInstance->Logs->Log("CULLLIST", DEBUG, "refcountbase::~ @%p with refcount %d",
			(void*)this, refcount);
	alloc_list_del(this);
}

usecountbase::~usecountbase()
{
	if (usecount && ServerInstance && ServerInstance->Logs)
		ServerInstance->Logs->Log("CULLLIST", DEBUG, "usecountbase::~ @%p with refcount %d",
			(void*)this, usecount);
}

ServiceProvider::~ServiceProvider()
{
}

ExtensionItem::ExtensionItem(ExtensibleType type, const std::string& Key, Module* mod)
	: ServiceProvider(mod, Key, SERVICE_METADATA), type_id(type)
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
	types.insert(std::make_pair(item->name, item));
}

void ExtensionManager::BeginUnregister(Module* module, std::vector<reference<ExtensionItem> >& list)
{
	std::map<std::string, reference<ExtensionItem> >::iterator i = types.begin();
	while (i != types.end())
	{
		std::map<std::string, reference<ExtensionItem> >::iterator me = i++;
		ExtensionItem* item = me->second;
		if (item->creator == module)
		{
			list.push_back(item);
			types.erase(me);
		}
	}
}

ExtensionItem* ExtensionManager::GetItem(const std::string& name)
{
	std::map<std::string, reference<ExtensionItem> >::iterator i = types.find(name);
	if (i == types.end())
		return NULL;
	return i->second;
}

void Extensible::doUnhookExtensions(const std::vector<reference<ExtensionItem> >& toRemove)
{
	for(std::vector<reference<ExtensionItem> >::const_iterator i = toRemove.begin(); i != toRemove.end(); ++i)
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

static struct DummyExtensionItem : LocalExtItem
{
	DummyExtensionItem() : LocalExtItem(EXTENSIBLE_NONE, "", NULL) {}
	void free(void*) {}
} dummy;

Extensible::Extensible(ExtensibleType Type) : type_id(Type)
{
	extensions[&dummy] = NULL;
}

CullResult Extensible::cull()
{
	for(ExtensibleStore::iterator i = extensions.begin(); i != extensions.end(); ++i)
	{
		i->first->free(i->second);
	}
	extensions.clear();
	return classbase::cull();
}

Extensible::~Extensible()
{
	if (!extensions.empty() && ServerInstance && ServerInstance->Logs)
		ServerInstance->Logs->Log("CULLLIST", DEBUG,
			"Extensible destructor called without cull @%p", (void*)this);
}

LocalExtItem::LocalExtItem(ExtensibleType type, const std::string& Key, Module* mod)
	: ExtensionItem(type, Key, mod)
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

LocalStringExt::LocalStringExt(ExtensibleType type, const std::string& Key, Module* Owner)
	: SimpleExtItem<std::string>(type, Key, Owner) { }

LocalStringExt::~LocalStringExt()
{
}

std::string LocalStringExt::serialize(SerializeFormat format, const Extensible* container, void* item) const
{
	if (item && format == FORMAT_USER)
		return *static_cast<std::string*>(item);
	return "";
}

LocalIntExt::LocalIntExt(ExtensibleType type, const std::string& Key, Module* mod)
	: LocalExtItem(type, Key, mod)
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

StringExtItem::StringExtItem(ExtensibleType type, const std::string& Key, Module* mod)
	: ExtensionItem(type, Key, mod)
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

ModuleException::ModuleException(const std::string &message, Module* who)
	: CoreException(message, who ? who->ModuleSourceFile : "A Module")
{
}

