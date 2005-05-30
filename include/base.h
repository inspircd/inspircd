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

#ifndef __BASE_H__ 
#define __BASE_H__ 

#include "inspircd_config.h" 
#include <time.h>
#include <map>
#include <string>

typedef void* VoidPointer;
 
/** The base class for all inspircd classes
*/ 
class classbase
{
 public:
 	/** Time that the object was instantiated (used for TS calculation etc)
 	*/
	time_t age;

	/** Constructor,
	 * Sets the object's time
	 */
	classbase() { age = time(NULL); }
	~classbase() { }
};

/** class Extensible is the parent class of many classes such as userrec and chanrec.
 * class Extensible implements a system which allows modules to 'extend' the class by attaching data within
 * a map associated with the object. In this way modules can store their own custom information within user
 * objects, channel objects and server objects, without breaking other modules (this is more sensible than using
 * a flags variable, and each module defining bits within the flag as 'theirs' as it is less prone to conflict and
 * supports arbitary data storage).
 */
class Extensible : public classbase
{
	/** Private data store
	 */
	std::map<std::string,char*> Extension_Items;
	
public:

	/** Extend an Extensible class.
	 *
	 * @param key The key parameter is an arbitary string which identifies the extension data
	 * @param p This parameter is a pointer to any data you wish to associate with the object
	 *
	 * You must provide a key to store the data as, and a void* to the data (typedef VoidPointer)
	 * The data will be inserted into the map. If the data already exists, you may not insert it
	 * twice, Extensible::Extend will return false in this case.
	 *
	 * @return Returns true on success, false if otherwise
	 */
	bool Extend(std::string key, char* p);

	/** Shrink an Extensible class.
	 *
	 * @param key The key parameter is an arbitary string which identifies the extension data
	 *
	 * You must provide a key name. The given key name will be removed from the classes data. If
	 * you provide a nonexistent key (case is important) then the function will return false.
	 *
	 * @return Returns true on success.
	 */
	bool Shrink(std::string key);
	
	/** Get an extension item.
	 *
	 * @param key The key parameter is an arbitary string which identifies the extension data
	 *
	 * @return If you provide a non-existent key name, the function returns NULL, otherwise a pointer to the item referenced by the key is returned.
	 */
	char* GetExt(std::string key);
};

/** BoolSet is a utility class designed to hold eight bools in a bitmask.
 * Use BoolSet::Set and BoolSet::Get to set and get bools in the bitmask,
 * and Unset and Invert for special operations upon them.
 */
class BoolSet
{
	char bits;

 public:

	/** The default constructor initializes the BoolSet to all values unset.
	 */
	BoolSet();

	/** This constructor copies the default bitmask from a char
	 */
	BoolSet(char bitmask);

	/** The Set method sets one bool in the set.
	 *
	 * @param number The number of the item to set. This must be between 0 and 7.
	 */
	void Set(int number);

	/** The Get method returns the value of one bool in the set
	 *
	 * @param number The number of the item to retrieve. This must be between 0 and 7.
	 *
	 * @return True if the item is set, false if it is unset.
	 */
	bool Get(int number);

	/** The Unset method unsets one value in the set
	 *
	 * @param number The number of the item to set. This must be between 0 and 7.
	 */
	void Unset(int number);

	/** The Unset method inverts (flips) one value in the set
	 *
	 * @param number The number of the item to invert. This must be between 0 and 7.
	 */
	void Invert(int number);

	/** Compare two BoolSets
	 */
	bool operator==(BoolSet other);

	/** OR two BoolSets together
	 */
	BoolSet operator|(BoolSet other);
	
	/** AND two BoolSets together
	 */
	BoolSet operator&(BoolSet other);

	/** Assign one BoolSet to another
	 */
	bool operator=(BoolSet other);
};


#endif

