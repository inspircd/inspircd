/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004-2006 Craig Edwards <craigedwards@brainbox.cc>
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


/* $Core */

#include "inspircd_config.h"
#include "base.h"
#include <time.h>
#include "inspircd.h"

const int bitfields[]           =       {1,2,4,8,16,32,64,128};
const int inverted_bitfields[]  =       {~1,~2,~4,~8,~16,~32,~64,~128};

classbase::classbase()
{
	this->age = time(NULL);
}

bool Extensible::Shrink(const std::string &key)
{
	/* map::size_type map::erase( const key_type& key );
	 * returns the number of elements removed, std::map
	 * is single-associative so this should only be 0 or 1
	 */
	return this->Extension_Items.erase(key);
}

void Extensible::GetExtList(std::deque<std::string> &list)
{
	for (ExtensibleStore::iterator u = Extension_Items.begin(); u != Extension_Items.end(); u++)
	{
		list.push_back(u->first);
	}
}

void BoolSet::Set(int number)
{
	this->bits |= bitfields[number];
}

void BoolSet::Unset(int number)
{
	this->bits &= inverted_bitfields[number];
}

void BoolSet::Invert(int number)
{
	this->bits ^= bitfields[number];
}

bool BoolSet::Get(int number)
{
	return ((this->bits | bitfields[number]) > 0);
}

bool BoolSet::operator==(BoolSet other)
{
	return (this->bits == other.bits);
}

BoolSet BoolSet::operator|(BoolSet other)
{
	BoolSet x(this->bits | other.bits);
	return x;
}

BoolSet BoolSet::operator&(BoolSet other)
{
	BoolSet x(this->bits & other.bits);
	return x;
}

BoolSet::BoolSet()
{
	this->bits = 0;
}

BoolSet::BoolSet(char bitmask)
{
	this->bits = bitmask;
}

bool BoolSet::operator=(BoolSet other)
{
	this->bits = other.bits;
	return true;
}
