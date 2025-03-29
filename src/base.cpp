/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020, 2022-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
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
#include "base.h"

// This trick detects heap allocations of refcountbase objects
static void* last_heap = nullptr;

void* refcountbase::operator new(size_t size)
{
	last_heap = ::operator new(size);
	return last_heap;
}

void refcountbase::operator delete(void* obj)
{
	if (last_heap == obj)
		last_heap = nullptr;
	::operator delete(obj);
}

refcountbase::refcountbase()
{
	if (this != last_heap)
		throw CoreException("Reference allocate on the stack!");
}

refcountbase::~refcountbase()
{
	if (refcount && ServerInstance)
	{
		ServerInstance->Logs.Debug("CULL", "refcountbase::~ @{} with refcount {}",
			FMT_PTR(this), refcount);
	}
}

usecountbase::~usecountbase()
{
	if (usecount && ServerInstance)
	{
		ServerInstance->Logs.Debug("CULL", "usecountbase::~ @{} with refcount {}",
			FMT_PTR(this), usecount);
	}
}

void ServiceProvider::RegisterService()
{
}
