/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2019, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
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
#include "base.h"

#ifdef INSPIRCD_ENABLE_RTTI
#include <typeinfo>
#endif

classbase::classbase() {
#ifdef INSPIRCD_ENABLE_RTTI
    if (ServerInstance) {
        ServerInstance->Logs->Log("CULLLIST", LOG_DEBUG, "classbase::+%s @%p",
                                  typeid(*this).name(), (void*)this);
    }
#endif
}

CullResult classbase::cull() {
#ifdef INSPIRCD_ENABLE_RTTI
    if (ServerInstance) {
        ServerInstance->Logs->Log("CULLLIST", LOG_DEBUG, "classbase::-%s @%p",
                                  typeid(*this).name(), (void*)this);
    }
#endif
    return CullResult();
}

classbase::~classbase() {
#ifdef INSPIRCD_ENABLE_RTTI
    if (ServerInstance) {
        ServerInstance->Logs->Log("CULLLIST", LOG_DEBUG, "classbase::~%s @%p",
                                  typeid(*this).name(), (void*)this);
    }
#endif
}

CullResult::CullResult() {
}

// This trick detects heap allocations of refcountbase objects
static void* last_heap = NULL;

void* refcountbase::operator new(size_t size) {
    last_heap = ::operator new(size);
    return last_heap;
}

void refcountbase::operator delete(void* obj) {
    if (last_heap == obj) {
        last_heap = NULL;
    }
    ::operator delete(obj);
}

refcountbase::refcountbase() : refcount(0) {
    if (this != last_heap) {
        throw CoreException("Reference allocate on the stack!");
    }
}

refcountbase::~refcountbase() {
    if (refcount && ServerInstance)
        ServerInstance->Logs->Log("CULLLIST", LOG_DEBUG,
                                  "refcountbase::~ @%p with refcount %d",
                                  (void*)this, refcount);
}

usecountbase::~usecountbase() {
    if (usecount && ServerInstance)
        ServerInstance->Logs->Log("CULLLIST", LOG_DEBUG,
                                  "usecountbase::~ @%p with refcount %d",
                                  (void*)this, usecount);
}

ServiceProvider::~ServiceProvider() {
}

void ServiceProvider::RegisterService() {
}

ExtensionItem::ExtensionItem(const std::string& Key, ExtensibleType exttype,
                             Module* mod)
    : ServiceProvider(mod, Key, SERVICE_METADATA)
    , type(exttype) {
}

ExtensionItem::~ExtensionItem() {
}

void* ExtensionItem::get_raw(const Extensible* container) const {
    Extensible::ExtensibleStore::const_iterator i =
        container->extensions.find(const_cast<ExtensionItem*>(this));
    if (i == container->extensions.end()) {
        return NULL;
    }
    return i->second;
}

void* ExtensionItem::set_raw(Extensible* container, void* value) {
    std::pair<Extensible::ExtensibleStore::iterator,bool> rv =
        container->extensions.insert(std::make_pair(this, value));
    if (rv.second) {
        return NULL;
    } else {
        void* old = rv.first->second;
        rv.first->second = value;
        return old;
    }
}

void* ExtensionItem::unset_raw(Extensible* container) {
    Extensible::ExtensibleStore::iterator i = container->extensions.find(this);
    if (i == container->extensions.end()) {
        return NULL;
    }
    void* rv = i->second;
    container->extensions.erase(i);
    return rv;
}

void ExtensionItem::RegisterService() {
    if (!ServerInstance->Extensions.Register(this)) {
        throw ModuleException("Extension already exists: " + name);
    }
}

bool ExtensionManager::Register(ExtensionItem* item) {
    return types.insert(std::make_pair(item->name, item)).second;
}

void ExtensionManager::BeginUnregister(Module* module,
                                       std::vector<reference<ExtensionItem> >& list) {
    ExtMap::iterator i = types.begin();
    while (i != types.end()) {
        ExtMap::iterator me = i++;
        ExtensionItem* item = me->second;
        if (item->creator == module) {
            list.push_back(item);
            types.erase(me);
        }
    }
}

ExtensionItem* ExtensionManager::GetItem(const std::string& name) {
    ExtMap::iterator i = types.find(name);
    if (i == types.end()) {
        return NULL;
    }
    return i->second;
}

void Extensible::doUnhookExtensions(const
                                    std::vector<reference<ExtensionItem> >& toRemove) {
    for(std::vector<reference<ExtensionItem> >::const_iterator i = toRemove.begin();
            i != toRemove.end(); ++i) {
        ExtensionItem* item = *i;
        ExtensibleStore::iterator e = extensions.find(item);
        if (e != extensions.end()) {
            item->free(this, e->second);
            extensions.erase(e);
        }
    }
}

Extensible::Extensible()
    : culled(false) {
}

CullResult Extensible::cull() {
    FreeAllExtItems();
    culled = true;
    return classbase::cull();
}

void Extensible::FreeAllExtItems() {
    for(ExtensibleStore::iterator i = extensions.begin(); i != extensions.end();
            ++i) {
        i->first->free(this, i->second);
    }
    extensions.clear();
}

Extensible::~Extensible() {
    if ((!extensions.empty() || !culled) && ServerInstance) {
        ServerInstance->Logs->Log("CULLLIST", LOG_DEBUG,
                                  "Extensible destructor called without cull @%p", (void*)this);
    }
}

void ExtensionItem::FromInternal(Extensible* container,
                                 const std::string& value) {
    FromNetwork(container, value);
}

void ExtensionItem::FromNetwork(Extensible* container,
                                const std::string& value) {
}

std::string ExtensionItem::ToHuman(const Extensible* container,
                                   void* item) const {
    // Try to use the network form by default.
    std::string ret = ToNetwork(container, item);

    // If there's no network form then fall back to the internal form.
    if (ret.empty()) {
        ret = ToInternal(container, item);
    }

    return ret;
}

std::string ExtensionItem::ToInternal(const Extensible* container,
                                      void* item) const {
    return ToNetwork(container, item);
}

std::string ExtensionItem::ToNetwork(const Extensible* container,
                                     void* item) const {
    return std::string();
}

std::string ExtensionItem::serialize(SerializeFormat format,
                                     const Extensible* container, void* item) const {
    // Wrap the deprecated API with the new API.
    switch (format) {
    case FORMAT_USER:
        return ToHuman(container, item);
    case FORMAT_INTERNAL:
    case FORMAT_PERSIST:
        return ToInternal(container, item);
    case FORMAT_NETWORK:
        return ToNetwork(container, item);
    }
    return "";
}


void ExtensionItem::unserialize(SerializeFormat format, Extensible* container,
                                const std::string& value) {
    // Wrap the deprecated API with the new API.
    switch (format) {
    case FORMAT_USER:
        break;
    case FORMAT_INTERNAL:
    case FORMAT_PERSIST:
        FromInternal(container, value);
        break;
    case FORMAT_NETWORK:
        FromNetwork(container, value);
        break;
    }
}

LocalStringExt::LocalStringExt(const std::string& Key, ExtensibleType exttype,
                               Module* Owner)
    : SimpleExtItem<std::string>(Key, exttype, Owner) {
}

LocalStringExt::~LocalStringExt() {
}

std::string LocalStringExt::ToInternal(const Extensible* container,
                                       void* item) const {
    return item ? *static_cast<std::string*>(item) : std::string();
}

void LocalStringExt::FromInternal(Extensible* container,
                                  const std::string& value) {
    set(container, value);
}

LocalIntExt::LocalIntExt(const std::string& Key, ExtensibleType exttype,
                         Module* mod)
    : ExtensionItem(Key, exttype, mod) {
}

LocalIntExt::~LocalIntExt() {
}

std::string LocalIntExt::ToInternal(const Extensible* container,
                                    void* item) const {
    return ConvToStr(reinterpret_cast<intptr_t>(item));
}

void LocalIntExt::FromInternal(Extensible* container,
                               const std::string& value) {
    set(container, ConvToNum<intptr_t>(value));
}

intptr_t LocalIntExt::get(const Extensible* container) const {
    return reinterpret_cast<intptr_t>(get_raw(container));
}

intptr_t LocalIntExt::set(Extensible* container, intptr_t value) {
    if (value) {
        return reinterpret_cast<intptr_t>(set_raw(container,
                                          reinterpret_cast<void*>(value)));
    } else {
        return reinterpret_cast<intptr_t>(unset_raw(container));
    }
}

void LocalIntExt::free(Extensible* container, void* item) {
}

StringExtItem::StringExtItem(const std::string& Key, ExtensibleType exttype,
                             Module* mod)
    : ExtensionItem(Key, exttype, mod) {
}

StringExtItem::~StringExtItem() {
}

std::string* StringExtItem::get(const Extensible* container) const {
    return static_cast<std::string*>(get_raw(container));
}

std::string StringExtItem::ToNetwork(const Extensible* container,
                                     void* item) const {
    return item ? *static_cast<std::string*>(item) : std::string();
}

void StringExtItem::FromNetwork(Extensible* container,
                                const std::string& value) {
    if (value.empty()) {
        unset(container);
    } else {
        set(container, value);
    }
}

void StringExtItem::set(Extensible* container, const std::string& value) {
    void* old = set_raw(container, new std::string(value));
    free(container, old);
}

void StringExtItem::unset(Extensible* container) {
    void* old = unset_raw(container);
    free(container, old);
}

void StringExtItem::free(Extensible* container, void* item) {
    delete static_cast<std::string*>(item);
}

ModuleException::ModuleException(const std::string &message, Module* who)
    : CoreException(message, who ? who->ModuleSourceFile : "A Module") {
}
