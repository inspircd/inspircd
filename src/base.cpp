#include "base.h"
#include "inspircd_config.h" 
#include <time.h>
#include <map>
#include <string>

bool Extensible::Extend(std::string key, VoidPointer p)
{
	// only add an item if it doesnt already exist
	if (this->Extension_Items.find(key) == this->Extension_Items.end())
	{
		this->Extension_Items[key] == p;
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
		return true;
	}
	return false;
}

VoidPointer Extensible::GetExt(std::string key)
{
	if (this->Extension_Items.find(key) != this->Extension_Items.end())
	{
		return (this->Extension_Items.find(key))->second;
	}
	return NULL;
}

