/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2003, 2006 Craig Edwards <craigedwards@brainbox.cc>
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
#ifdef _WIN32
		RetrieveLastError();
#else
		err = dlerror();
#endif
	}
}

DLLManager::~DLLManager()
{
	/* close the library */
	if (h)
		dlclose(h);
}

union init_t {
	void* vptr;
	Module* (*fptr)();
};

Module* DLLManager::CallInit()
{
	if (!h)
		return NULL;

	init_t initfn;
	initfn.vptr = dlsym(h, MODULE_INIT_STR);
	if (!initfn.vptr)
	{
#ifdef _WIN32
		RetrieveLastError();
#else
		err = dlerror();
#endif
		return NULL;
	}

	return (*initfn.fptr)();
}

std::string DLLManager::GetVersion()
{
	if (!h)
		return "";

	const char* srcver = (char*)dlsym(h, "inspircd_src_version");
	if (srcver)
		return srcver;
	return "Unversioned module";
}

#ifdef _WIN32
void DLLManager::RetrieveLastError()
{
	char errmsg[500];
	DWORD dwErrorCode = GetLastError();
	if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)errmsg, _countof(errmsg), NULL) == 0)
		sprintf_s(errmsg, _countof(errmsg), "Error code: %u", dwErrorCode);
	SetLastError(ERROR_SUCCESS);
	err = errmsg;

	std::string::size_type p;
	while ((p = err.find_last_of("\r\n")) != std::string::npos)
		err.erase(p, 1);
}
#endif
