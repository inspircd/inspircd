/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2020-2022, 2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2011-2012 Adam <Adam@anope.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Oliver Lupton <om@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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


#pragma once

#include "compat.h"
#include <string>

#include "utility/uncopiable.h"
#include "cull.h"

/** Base class for use count tracking. Uses reference<>, but does not
 * cause object deletion when the last user is removed.
 *
 * Safe for use as a second parent class; will not add a second vtable.
 */
class CoreExport usecountbase
	: private insp::uncopiable
{
	mutable unsigned int usecount = 0;
public:
	usecountbase() = default;
	inline unsigned int GetUseCount() const { return usecount; }
	inline void refcount_inc() const { usecount++; }
	inline bool refcount_dec() const { usecount--; return false; }
};

template <typename T>
class reference final
{
	T* value = nullptr;
public:
	reference() = default;
	reference(T* v) : value(v) { if (value) value->refcount_inc(); }
	reference(const reference<T>& v) : value(v.value) { if (value) value->refcount_inc(); }
	reference<T>& operator=(const reference<T>& other)
	{
		if (other.value)
			other.value->refcount_inc();
		this->reference::~reference();
		value = other.value;
		return *this;
	}

	~reference()
	{
		if (value && value->refcount_dec())
			delete value;
	}

	inline reference<T>& operator=(T* other)
	{
		if (value != other)
		{
			if (value && value->refcount_dec())
				delete value;
			value = other;
			if (value)
				value->refcount_inc();
		}

		return *this;
	}

	inline T* ptr() const { return value; }
	inline operator bool() const { return (value != nullptr); }
	inline operator T*() const { return value; }
	inline T* operator->() const { return value; }
	inline T& operator*() const { return *value; }
	inline bool operator<(const reference<T>& other) const { return value < other.value; }
	inline bool operator>(const reference<T>& other) const { return value > other.value; }
	static inline void* operator new(size_t, void* m) { return m; }
private:
#ifndef _WIN32
	static void* operator new(size_t);
	static void operator delete(void*);
#endif
};
