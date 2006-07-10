/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd_config.h"
#include "base.h"
#include <time.h>
#include <map>
#include <deque>
#include <string>
#include "inspircd.h"
#include "modules.h"
#include "helperfuncs.h"

const int bitfields[]           =       {1,2,4,8,16,32,64,128};
const int inverted_bitfields[]  =       {~1,~2,~4,~8,~16,~32,~64,~128};

extern time_t TIME;

bool Extensible::Shrink(const std::string &key)
{
	/* map::size_type map::erase( const key_type& key );
	 * returns the number of elements removed, std::map
	 * is single-associative so this should only be 0 or 1
	 */
	if(this->Extension_Items.erase(key))
	{
		log(DEBUG, "Shrinking object with item %s",key.c_str());
		return true;
	}
	else
	{
		log(DEBUG, "Tried to shrink object with item %s but no items removed", key.c_str());		
		return false;
	}
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
