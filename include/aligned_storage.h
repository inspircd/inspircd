/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

namespace insp {
template <typename T> class aligned_storage;
}

template <typename T>
class insp::aligned_storage {
    mutable typename
    TR1NS::aligned_storage<sizeof(T), TR1NS::alignment_of<T>::value>::type data;

  public:
    aligned_storage() {
    }

    aligned_storage(const aligned_storage& other) {
    }

    T* operator->() const {
        return static_cast<T*>(static_cast<void*>(&data));
    }

    operator T*() const {
        return operator->();
    }
};
