/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2010 Craig Edwards <brain@inspircd.org>
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
#include <dlfcn.h>
#else
#define dlopen(path, state) (void*)LoadLibraryA(path)
#define dlsym(handle, export) (void*)GetProcAddress((HMODULE)handle, export)
#define dlclose(handle) FreeLibrary((HMODULE)handle)
#endif

DLLManager::DLLManager(const char *fname)
{
	if (!strstr(fname,".so"))
	{
		err = "This doesn't look like a module file to me...";
		h = NULL;
		return;
	}

	h = dlopen(fname, RTLD_NOW|RTLD_LOCAL);
	if (!h)
	{
		RetrieveLastError();
	}
}

DLLManager::~DLLManager()
{
	/* close the library */
	if (h)
		dlclose(h);
}

Module* DLLManager::CallInit()
{
	union
	{
		void* vptr;
		Module* (*fptr)();
	};

	vptr = GetSymbol(MODULE_INIT_STR);
	if (!vptr)
		return NULL;

	return (*fptr)();
}

void* DLLManager::GetSymbol(const char* name)
{
	return h ? dlsym(h, name) : NULL;
}

std::string DLLManager::GetVersion()
{
	const char* srcver = static_cast<const char*>(GetSymbol("inspircd_src_version"));
	return srcver ? srcver : "";
}

void DLLManager::RetrieveLastError()
{
#if defined _WIN32
	char errmsg[500];
	DWORD dwErrorCode = GetLastError();
	if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)errmsg, _countof(errmsg), NULL) == 0)
		sprintf_s(errmsg, _countof(errmsg), "Error code: %u", dwErrorCode);
	SetLastError(ERROR_SUCCESS);
	err = errmsg;
#else
	const char* errmsg = dlerror();
	err = errmsg ? errmsg : "Unknown error";
#endif

	std::string::size_type p;
	while ((p = err.find_last_of("\r\n")) != std::string::npos)
		err.erase(p, 1);
}
