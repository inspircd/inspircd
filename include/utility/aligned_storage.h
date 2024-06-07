/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2022, 2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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

namespace insp
{
	template <typename T> class aligned_storage;
}

/** A block of preallocated memory which an object can be created into. */
template <typename T>
class insp::aligned_storage final
{
private:
	/** The underlying aligned storage block. */
	alignas(T) mutable std::byte data[sizeof(T)];

public:
	/** Default constructor for the aligned_storage class. */
	aligned_storage() = default;

	/** Ignores copying via the copy constructor. */
	aligned_storage(const aligned_storage&) { }

	/** Accessor for the underlying aligned storage block. */
	T* operator->() const { return static_cast<T*>(static_cast<void*>(&data)); }

	/** Constant accessor for the underlying aligned storage block. */
	operator T*() const { return operator->(); }
};
