/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020, 2022 Sadie Powell <sadie@witchery.services>
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

#include <iterator>

namespace insp
{

struct intrusive_list_def_tag { };

template <typename T, typename Tag = intrusive_list_def_tag> class intrusive_list;
template <typename T, typename Tag = intrusive_list_def_tag> class intrusive_list_tail;

template <typename T, typename Tag = intrusive_list_def_tag>
class intrusive_list_node
{
	T* ptr_next = nullptr;
	T* ptr_prev = nullptr;

	void unlink()
	{
		if (ptr_next)
			ptr_next->intrusive_list_node<T, Tag>::ptr_prev = this->ptr_prev;
		if (ptr_prev)
			ptr_prev->intrusive_list_node<T, Tag>::ptr_next = this->ptr_next;
		ptr_next = ptr_prev = nullptr;
	}

public:
	friend class intrusive_list<T, Tag>;
	friend class intrusive_list_tail<T, Tag>;
};

} // namespace insp

// Intrusive list where the list only has a pointer to the head element
#define INSPIRCD_INTRUSIVE_LIST_NAME intrusive_list
#include "intrusive_list_impl.h"
#undef INSPIRCD_INTRUSIVE_LIST_NAME

// Intrusive list where the list maintains a pointer to both the head and the tail elements.
// Additional methods: back(), push_back(), pop_back()
#define INSPIRCD_INTRUSIVE_LIST_NAME intrusive_list_tail
#define INSPIRCD_INTRUSIVE_LIST_HAS_TAIL
#include "intrusive_list_impl.h"
#undef INSPIRCD_INTRUSIVE_LIST_NAME
#undef INSPIRCD_INTRUSIVE_LIST_HAS_TAIL
