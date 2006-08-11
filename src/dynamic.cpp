/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
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

#include "inspircd_config.h"
#include "configreader.h"
#include "globals.h"
#include "dynamic.h"

#ifndef STATIC_LINK
#include <dlfcn.h>
#else
#include "modlist.h"
#endif

#include "inspstring.h"
#include "helperfuncs.h"
#include "inspircd.h"
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>

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
	// Copy the library to a temp location, this makes recompiles
	// a little safer if the ircd is running at the time as the
	// shared libraries are mmap()ed and not doing this causes
	// segfaults.
	FILE* x = fopen(fname,"rb");
	if (!x)
	{
		err = strerror(errno);
		return;
	}
	log(DEBUG,"Opened module file %s",fname);
	char tmpfile_template[255];
	char buffer[65536];
	snprintf(tmpfile_template, 255, "%s/inspircd_file.so.%d.XXXXXXXXXX",ServerInstance->Config->TempDir,getpid());
	int fd = mkstemp(tmpfile_template);
	if (fd == -1)
	{
		fclose(x);
		err = strerror(errno);
		return;
	}
	log(DEBUG,"Copying %s to %s",fname, tmpfile_template);
	while (!feof(x))
	{
		int n = fread(buffer, 1, 65535, x);
		if (n)
		{
			int written = write(fd,buffer,n);
			if (written != n)
			{
				fclose(x);
				err = strerror(errno);
				return;
			}
		}
	}
	log(DEBUG,"Copied entire file.");
	// Try to open the library now and get any error message.

	if (close(fd) == -1)
		err = strerror(errno);
	if (fclose(x) == EOF)
		err = strerror(errno);

	h = dlopen(fname, RTLD_NOW|RTLD_LOCAL);
	if (!h)
	{
		log(DEBUG,"dlerror occured!");
		err = (char*)dlerror();
		return;
	}

	log(DEBUG,"Finished loading '%s': %0x",tmpfile_template, h);

	// We can delete the tempfile once it's loaded, leaving just the inode.
	if (!err && !ServerInstance->Config->debugging)
	{
		log(DEBUG,"Deleteting %s",tmpfile_template);
		if (unlink(tmpfile_template) == -1)
			err = strerror(errno);
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

