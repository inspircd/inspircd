/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "dynamic.h"
#ifndef WIN32
#include <dlfcn.h>
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
		err = dlerror();
	}
}

#ifdef VT_DEBUG
extern std::set<void*>* alloc_list;
static void check_list(void* h)
{
	Dl_info info;
	void* ifn = dlsym(h, MODULE_INIT_STR);
	if (!ifn)
		return;
	if (!dladdr(ifn, &info))
		return;
	std::string soname = info.dli_fname;
	for(std::set<void*>::iterator i = alloc_list->begin(); i != alloc_list->end(); i++)
	{
		void* vtable = *reinterpret_cast<void**>(*i);
		if (dladdr(vtable, &info) && info.dli_fname == soname)
		{
			ServerInstance->Logs->Log("DLLMGR", DEBUG, "Object @%p remains with vtable %s+0x%lx <%p> in %s",
				*i, info.dli_sname, (long)(vtable - info.dli_saddr), vtable, info.dli_fname);
		}
	}
}

#else
#define check_list(h) do {} while (0)
#endif

DLLManager::~DLLManager()
{
	/* close the library */
	if (h)
	{
		check_list(h);
		dlclose(h);
	}
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
		err = dlerror();
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
