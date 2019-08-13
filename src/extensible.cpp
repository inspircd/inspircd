/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Peter Powell <petpow@saberuk.com>
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


StringExtItem::StringExtItem(Module* owner, const std::string& key, ExtensibleType exttype, bool sync)
	: SimpleExtItem(owner, key, exttype)
	, synced(sync)
{
}

void StringExtItem::FromInternal(Extensible* container, const std::string& value)
{
	if (value.empty())
		unset(container);
	else
		set(container, value);
}

void StringExtItem::FromNetwork(Extensible* container, const std::string& value)
{
	if (synced)
		FromInternal(container, value);
}

std::string StringExtItem::ToInternal(const Extensible* container, void* item) const
{
	return item ? *static_cast<std::string*>(item) : std::string();
}

std::string StringExtItem::ToNetwork(const Extensible* container, void* item) const
{
	return synced ? ToInternal(container, item) : std::string();
}
