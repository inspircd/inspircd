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

#include "globals.h"
#include <dlfcn.h>
#include "dynamic.h"
#include "inspstring.h"
#include "helperfuncs.h"

DLLManager::DLLManager(const char *fname)
{
    // Try to open the library now and get any error message.
	
	h=dlopen( fname, RTLD_NOW );
	err=dlerror();
}

DLLManager::~DLLManager()
{
	// close the library if it isn't null
	if (h!=0)
    	dlclose(h);
}


bool DLLManager::GetSymbol(void **v, const char *sym_name)
{
	// try extract a symbol from the library
	// get any error message is there is any
	
	if( h!=0 )
	{
		*v = dlsym( h, sym_name );
    	err=dlerror();
	    if( err==0 )
       	  return true;
    	else
    	  return false;
	}
	else
	{	
        return false;
	}
	
}


DLLFactoryBase::DLLFactoryBase(const char *fname, const char *factory) : DLLManager(fname)
{
	// try get the factory function if there is no error yet
	
	factory_func=0;
	
	if( LastError()==0 )
	{		
    	GetSymbol( (void **)&factory_func, factory ? factory : "init_module" );
	}
	
}


DLLFactoryBase::~DLLFactoryBase()
{
}



