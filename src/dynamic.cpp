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

#include "configreader.h"
#include "dynamic.h"

#ifndef STATIC_LINK
#include <dlfcn.h>
#else
#include "modlist.h"
#endif

#include "inspircd.h"

DLLManager::DLLManager(InspIRCd* ServerInstance, const char *fname)
{
	err = NULL;

	if (!strstr(fname,".so"))
	{
		err = "This doesn't look like a module file to me...";
		return;
	}
#ifdef STATIC_LINK
	this->staticname[0] = '\0';
	for (int j = 0; modsyms[j].name; j++)
	{
		if (!strcmp(modsyms[j].name,fname))
		{
			strlcpy(this->staticname,fname,1020);
			err = 0;
			return;
		}
	}
	err = "Module is not statically compiled into the ircd";
#else
	h = dlopen(fname, RTLD_NOW|RTLD_LOCAL);
	if (!h)
	{
		err = (char*)dlerror();
		return;
	}
#endif
}

DLLManager::~DLLManager()
{
#ifndef STATIC_LINK
	// close the library if it isn't null
	if (h)
		dlclose(h);
#endif
}



#ifdef STATIC_LINK

bool DLLManager::GetSymbol(initfunc* &v, const char *sym_name)
{
	for (int j = 0; modsyms[j].name; j++)
	{
		if (!strcmp(this->staticname,modsyms[j].name))
		{
			v = modsyms[j].value;
			err = 0;
			return true;
		}
	}
	err = "Module symbol missing from the core";
	return false;
}

#else

bool DLLManager::GetSymbol(void** v, const char* sym_name)
{
	// try extract a symbol from the library
	// get any error message is there is any
	
	if (h)
	{
		dlerror(); // clear value
		*v = dlsym(h, sym_name);
		err = (char*)dlerror();
		if (!*v || err)
			return false;
	}
	
	if (err)
	{
		return false;
	}
	else
	{	
		return true;
	}
}

#endif

DLLFactoryBase::DLLFactoryBase(InspIRCd* Instance, const char* fname, const char* symbol) : DLLManager(Instance, fname)
{
	// try get the factory function if there is no error yet
	factory_func = 0;
	
	if (!LastError())
	{
#ifdef STATIC_LINK
		if (!GetSymbol( factory_func, symbol ? symbol : "init_module"))
#else
		if (!GetSymbol( (void **)&factory_func, symbol ? symbol : "init_module"))
#endif
		{
			throw ModuleException("Missing init_module() entrypoint!");
		}
	}
}

DLLFactoryBase::~DLLFactoryBase()
{
}

