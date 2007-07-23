/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "configreader.h"
#include "dynamic.h"
#ifndef WIN32
#include <dlfcn.h>
#endif

DLLManager::DLLManager(InspIRCd* ServerInstance, const char *fname)
{
	err = NULL;

	if (!strstr(fname,".so"))
	{
		err = "This doesn't look like a module file to me...";
		return;
	}

	h = dlopen(fname, RTLD_NOW|RTLD_LOCAL);
	if (!h)
	{
		err = (char*)dlerror();
		return;
	}
}

DLLManager::~DLLManager()
{
	/* close the library */
	if (h)
		dlclose(h);
}



bool DLLManager::GetSymbol(void** v, const char* sym_name)
{
	/*
	 * try extract a symbol from the library
	 * get any error message is there is any
	 */
	
	if (h)
	{
		dlerror(); // clear value
		*v = dlsym(h, sym_name);
		err = (char*)dlerror();
		if (!*v || err)
			return false;
	}
	
	/* succeeded :) */
	return true;
}

DLLFactoryBase::DLLFactoryBase(InspIRCd* Instance, const char* fname, const char* symbol) : DLLManager(Instance, fname)
{
	/* try get the factory function if there is no error yet */
	factory_func = 0;
	
	if (!LastError())
	{
		if (!GetSymbol( (void **)&factory_func, symbol ? symbol : "init_module"))
		{
			throw ModuleException("Missing init_module() entrypoint!");
		}
	}
}

DLLFactoryBase::~DLLFactoryBase()
{
}

