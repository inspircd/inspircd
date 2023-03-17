/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2017-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2003, 2006 Craig Edwards <brain@inspircd.org>
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
#ifndef _WIN32
# include <dlfcn.h>
#endif

/** The extension that dynamic libraries end with. */
#define DLL_EXTENSION ".so"

DLLManager::DLLManager(const std::string& name)
    : lib(NULL)
    , libname(name) {
    static size_t extlen = strlen(DLL_EXTENSION);
    if (name.length() <= extlen
            || name.compare(name.length() - extlen, name.length(), DLL_EXTENSION)) {
        err.assign(name + " is not a module (no " DLL_EXTENSION " extension)");
        return;
    }

#ifdef _WIN32
    lib = LoadLibraryA(name.c_str());
#else
    lib = dlopen(name.c_str(), RTLD_NOW|RTLD_LOCAL);
#endif

    if (!lib) {
        RetrieveLastError();
    }
}

DLLManager::~DLLManager() {
    if (!lib) {
        return;
    }

#ifdef _WIN32
    FreeLibrary(lib);
#else
    dlclose(lib);
#endif
}

Module* DLLManager::CallInit() {
    if (!lib) {
        return NULL;
    }

    const unsigned long* abi = GetSymbol<const unsigned long>(MODULE_STR_ABI);
    if (!abi) {
        err.assign(libname + " is not a module (no ABI symbol)");
        return NULL;
    } else if (*abi != MODULE_ABI) {
        const char* version = GetVersion();
        err.assign(
            InspIRCd::Format("%s was built against %s (%lu) which is too %s to use with %s (%lu).",
                             libname.c_str(), version ? version : "an unknown version", *abi,
                             *abi < MODULE_ABI ? "old" : "new", INSPIRCD_VERSION, MODULE_ABI));
        return NULL;
    }

    union {
        void* vptr;
        Module* (*fptr)();
    };

    vptr = GetSymbol(MODULE_STR_INIT);
    if (!vptr) {
        err.assign(libname + " is not a module (no init symbol)");
        return NULL;
    }

    return (*fptr)();
}

void* DLLManager::GetSymbol(const char* name) const {
    if (!lib) {
        return NULL;
    }

#if defined _WIN32
    return GetProcAddress(lib, name);
#else
    return dlsym(lib, name);
#endif
}

void DLLManager::RetrieveLastError() {
#ifdef _WIN32
    err = GetErrorMessage(GetLastError());
    SetLastError(ERROR_SUCCESS);
#else
    const char* errmsg = dlerror();
    err = errmsg ? errmsg : "Unknown error";
#endif

    for (size_t pos = 0;
            ((pos = err.find_first_of("\r\n", pos)) != std::string::npos); ) {
        err[pos] = ' ';
    }
}
