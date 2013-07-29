/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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
#include <gcrypt.h>

#ifdef _WIN32
# pragma comment(lib, "libgcrypt.lib")
#endif

/* $CompileFlags: exec("libgcrypt-config --cflags") */
/* $LinkerFlags: exec("libgcrypt-config --libs") */

class RandGen : public HandlerBase2<void, char*, size_t>
{
 public:
	RandGen() {}
	void Call(char* buffer, size_t len)
	{
		gcry_randomize(buffer, len, GCRY_STRONG_RANDOM);
	}
};

class ModuleRandGCrypt : public Module
{
	RandGen randhandler;

 public:
	ModuleRandGCrypt()
	{
		gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->GenRandom = &randhandler;
	}	
	
	~ModuleRandGCrypt()
	{
		ServerInstance->GenRandom = &ServerInstance->HandleGenRandom;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides a secure random number generator.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRandGCrypt)
