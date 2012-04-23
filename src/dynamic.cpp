/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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


/* $Core */

#include "inspircd.h"
#include "dynamic.h"
#ifndef WIN32
#include <dlfcn.h>
#endif

DLLManager::DLLManager(InspIRCd*, const char *fname)
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
