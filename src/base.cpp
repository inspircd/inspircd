/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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

#include "base.h"
#include "inspircd_config.h" 
#include <time.h>
#include <map>
#include <string>
#include "inspircd.h"
#include "modules.h"

extern time_t TIME;

bool Extensible::Extend(std::string key, char* p)
{
	// only add an item if it doesnt already exist
	if (this->Extension_Items.find(key) == this->Extension_Items.end())
	{
		this->Extension_Items[key] = p;
		log(DEBUG,"Extending object with item %s",key.c_str());
		return true;
	}
	// item already exists, return false
	return false;
}

bool Extensible::Shrink(std::string key)
{
	// only attempt to remove a map item that exists
	if (this->Extension_Items.find(key) != this->Extension_Items.end())
	{
		this->Extension_Items.erase(this->Extension_Items.find(key));
		log(DEBUG,"Shrinking object with item %s",key.c_str());
		return true;
	}
	return false;
}

char* Extensible::GetExt(std::string key)
{
	log(DEBUG,"Checking extension items for %s",key.c_str());
	if (this->Extension_Items.find(key) != this->Extension_Items.end())
	{
		log(DEBUG,"Found item %s %s",key.c_str(),(this->Extension_Items.find(key))->second);
		return (this->Extension_Items.find(key))->second;
	}
	log(DEBUG,"Cant find item %s",key.c_str());
	return NULL;
}

