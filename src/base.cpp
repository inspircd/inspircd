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

/* $Core */

#include "inspircd_config.h"
#include "base.h"
#include <time.h>
#include "inspircd.h"

const int bitfields[]           =       {1,2,4,8,16,32,64,128};
const int inverted_bitfields[]  =       {~1,~2,~4,~8,~16,~32,~64,~128};
std::map<std::string, ExtensionItem*> Extensible::extension_types;

classbase::classbase()
{
}

void classbase::cull()
{
}

classbase::~classbase()
{
}

void BoolSet::Set(int number)
{
	this->bits |= bitfields[number];
}

void BoolSet::Unset(int number)
{
	this->bits &= inverted_bitfields[number];
}

void BoolSet::Invert(int number)
{
	this->bits ^= bitfields[number];
}

bool BoolSet::Get(int number)
{
	return ((this->bits | bitfields[number]) > 0);
}

bool BoolSet::operator==(BoolSet other)
{
	return (this->bits == other.bits);
}

BoolSet BoolSet::operator|(BoolSet other)
{
	BoolSet x(this->bits | other.bits);
	return x;
}

BoolSet BoolSet::operator&(BoolSet other)
{
	BoolSet x(this->bits & other.bits);
	return x;
}

BoolSet::BoolSet()
{
	this->bits = 0;
}

BoolSet::BoolSet(char bitmask)
{
	this->bits = bitmask;
}

bool BoolSet::operator=(BoolSet other)
{
	this->bits = other.bits;
	return true;
}

ExtensionItem::ExtensionItem(const std::string& Key, Module* mod) : key(Key), owner(mod)
{
}

void* ExtensionItem::get_raw(const Extensible* container)
{
	ExtensibleStore::const_iterator i = container->extensions.find(key);
	if (i == container->extensions.end())
		return NULL;
	return i->second;
}

void* ExtensionItem::set_raw(Extensible* container, void* value)
{
	std::pair<ExtensibleStore::iterator,bool> rv = 
		container->extensions.insert(std::make_pair(key, value));
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
	ExtensibleStore::iterator i = container->extensions.find(key);
	if (i == container->extensions.end())
		return NULL;
	void* rv = i->second;
	container->extensions.erase(i);
	return rv;
}

bool Extensible::Register(ExtensionItem* item)
{
	return Extensible::extension_types.insert(std::make_pair(item->key, item)).second;
}

std::vector<ExtensionItem*> Extensible::BeginUnregister(Module* module)
{
	std::vector<ExtensionItem*> rv;
	ExtensibleTypes::iterator i = extension_types.begin();
	while (i != extension_types.end())
	{
		ExtensibleTypes::iterator c = i++;
		if (c->second->owner == module)
		{
			rv.push_back(c->second);
			extension_types.erase(c);
		}
	}
	return rv;
}

void Extensible::doUnhookExtensions(const std::vector<ExtensionItem*>& toRemove)
{
	for(std::vector<ExtensionItem*>::const_iterator i = toRemove.begin(); i != toRemove.end(); i++)
	{
		ExtensibleStore::iterator e = extensions.find((**i).key);
		if (e != extensions.end())
		{
			(**i).free(e->second);
			extensions.erase(e);
		}
	}
}

Extensible::~Extensible()
{
	for(ExtensibleStore::iterator i = extensions.begin(); i != extensions.end(); ++i)
	{
		ExtensionItem* type = GetItem(i->first);
		if (type)
			type->free(i->second);	
		else if (ServerInstance && ServerInstance->Logs)
			ServerInstance->Logs->Log("BASE", ERROR, "Extension type %s is not registered", i->first.c_str());
	}
}

LocalExtItem::LocalExtItem(const std::string& Key, Module* mod) : ExtensionItem(Key, mod)
{
}

std::string LocalExtItem::serialize(SerializeFormat format, const Extensible* container, void* item)
{
	return "";
}

void LocalExtItem::unserialize(SerializeFormat format, Extensible* container, const std::string& value)
{
}

LocalStringExt::LocalStringExt(const std::string& Key, Module* Owner)
	: SimpleExtItem<std::string>(Key, Owner) { }

std::string LocalStringExt::serialize(SerializeFormat format, const Extensible* container, void* item)
{
	if (item && format == FORMAT_USER)
		return *static_cast<std::string*>(item);
	return "";
}

LocalIntExt::LocalIntExt(const std::string& Key, Module* mod) : LocalExtItem(Key, mod)
{
}

std::string LocalIntExt::serialize(SerializeFormat format, const Extensible* container, void* item)
{
	if (format != FORMAT_USER)
		return "";
	return ConvToStr(reinterpret_cast<intptr_t>(item));
}

intptr_t LocalIntExt::get(const Extensible* container)
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

std::string* StringExtItem::get(const Extensible* container)
{
	return static_cast<std::string*>(get_raw(container));
}

std::string StringExtItem::serialize(SerializeFormat format, const Extensible* container, void* item)
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
