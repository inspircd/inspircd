/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "globals.h"
#include "inspircd_config.h"
#include "dynamic.h"

#ifndef STATIC_LINK
#include <dlfcn.h>
#else
#include "modlist.h"
#endif

#include "inspstring.h"
#include "helperfuncs.h"

DLLManager::DLLManager(const char *fname)
{
#ifdef STATIC_LINK
	this->staticname[0] = '\0';
	log(DEBUG,"Loading core-compiled module '%s'",fname);
	for (int j = 0; modsyms[j].name; j++)
	{
		log(DEBUG,"Check %s",modsyms[j].name);
		if (!strcmp(modsyms[j].name,fname))
		{
			log(DEBUG,"Found %s",fname);
			strlcpy(this->staticname,fname,1020);
			err = 0;
			return;
		}
	}
	err = "Module is not statically compiled into the ircd";
#else
    // Try to open the library now and get any error message.
	
	h = dlopen( fname, RTLD_NOW );
	err = dlerror();
#endif
}

DLLManager::~DLLManager()
{
#ifndef STATIC_LINK
	// close the library if it isn't null
	if (h != 0)
	dlclose(h);
#endif
}



#ifdef STATIC_LINK

bool DLLManager::GetSymbol(initfunc* &v, const char *sym_name)
{
	log(DEBUG,"Symbol search...");
	for (int j = 0; modsyms[j].name; j++)
	{
		if (!strcmp(this->staticname,modsyms[j].name))
		{
			log(DEBUG,"Loading symbol...");
			v = modsyms[j].value;
			err = 0;
			return true;
		}
	}
	err = "Module symbol missing from the core";
	return false;
}

#else

bool DLLManager::GetSymbol(void **v, const char *sym_name)
{
	// try extract a symbol from the library
	// get any error message is there is any
	
	if(h != 0)
	{
		*v = dlsym( h, sym_name );
		err = dlerror();
		if( err == 0 )
			return true;
	    	else
			return false;
	}
	else
	{	
		return false;
	}
}

#endif

DLLFactoryBase::DLLFactoryBase(const char *fname, const char *factory) : DLLManager(fname)
{
	// try get the factory function if there is no error yet
	
	factory_func = 0;
	
	if(LastError() == 0)
	{
#ifdef STATIC_LINK
		GetSymbol( factory_func, factory ? factory : "init_module" );
#else
		GetSymbol( (void **)&factory_func, factory ? factory : "init_module" );
#endif
	}
}


DLLFactoryBase::~DLLFactoryBase()
{
}
