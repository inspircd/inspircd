/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 Sadie Powell <sadie@witchery.services>
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
	/** Determines if the pointer in \p ptr1 is empty.
	 * @param ptr Either a shared_ptr<> or a weak_ptr<>.
	 */
	template <typename Ptr>
	bool empty_ptr(const Ptr& ptr)
	{
		return same_ptr(ptr, {});
	}

	/** Determines if the pointer in \p ptr1 points to the same value as the pointer in \p ptr2.
	 * @param ptr1 Either a shared_ptr<> or a weak_ptr<>.
	 * @param ptr2 Either a shared_ptr<> or a weak_ptr<>.
	 */
	template <typename Ptr1, typename Ptr2 = Ptr1>
	bool same_ptr(const Ptr1& ptr1, const Ptr2& ptr2)
	{
		return !ptr1.owner_before(ptr2) && !ptr2.owner_before(ptr1);
	}
}
