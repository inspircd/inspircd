/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
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
#include <deque>
#include <string>

/** Do we use this? -- Brain */
typedef void* VoidPointer;

/** A private data store for an Extensible class */
typedef std::map<std::string,char*> ExtensibleStore;

/** Needed */
class InspIRCd;

/** The base class for all inspircd classes.
 * Wherever possible, all classes you create should inherit from this,
 * giving them the ability to be passed to various core functions
 * as 'anonymous' classes.
*/ 
class CoreExport classbase
{
 public:
 	/** Time that the object was instantiated (used for TS calculation etc)
 	*/
	time_t age;

	/** Constructor.
	 * Sets the object's time
	 */
	classbase();

	/** Destructor.
	 * Does sweet FA.
	 */
	virtual ~classbase() { }
};

/** class Extensible is the parent class of many classes such as User and Channel.
 * class Extensible implements a system which allows modules to 'extend' the class by attaching data within
 * a map associated with the object. In this way modules can store their own custom information within user
 * objects, channel objects and server objects, without breaking other modules (this is more sensible than using
 * a flags variable, and each module defining bits within the flag as 'theirs' as it is less prone to conflict and
 * supports arbitary data storage).
 */
class CoreExport Extensible : public classbase
{
	/** Private data store.
	 * Holds all extensible metadata for the class.
	 */
	ExtensibleStore Extension_Items;
	
public:

	/** Extend an Extensible class.
	 *
	 * @param key The key parameter is an arbitary string which identifies the extension data
	 * @param p This parameter is a pointer to any data you wish to associate with the object
	 *
	 * You must provide a key to store the data as via the parameter 'key' and store the data
	 * in the templated parameter 'p'.
	 * The data will be inserted into the map. If the data already exists, you may not insert it
	 * twice, Extensible::Extend will return false in this case.
	 *
	 * @return Returns true on success, false if otherwise
	 */
	template<typename T> bool Extend(const std::string &key, T* p)
	{
		/* This will only add an item if it doesnt already exist,
		 * the return value is a std::pair of an iterator to the
		 * element, and a bool saying if it was actually inserted.
		 */
		return this->Extension_Items.insert(std::make_pair(key, (char*)p)).second;
	}

	/** Extend an Extensible class.
	 *
	 * @param key The key parameter is an arbitary string which identifies the extension data
	 *
	 * You must provide a key to store the data as via the parameter 'key', this single-parameter
	 * version takes no 'data' parameter, this is used purely for boolean values.
	 * The key will be inserted into the map with a NULL 'data' pointer. If the key already exists
	 * then you may not insert it twice, Extensible::Extend will return false in this case.
	 *
	 * @return Returns true on success, false if otherwise
	 */
	bool Extend(const std::string &key)
	{
		/* This will only add an item if it doesnt already exist,
		 * the return value is a std::pair of an iterator to the
		 * element, and a bool saying if it was actually inserted.
		 */
		return this->Extension_Items.insert(std::make_pair(key, (char*)NULL)).second;
	}

	/** Shrink an Extensible class.
	 *
	 * @param key The key parameter is an arbitary string which identifies the extension data
	 *
	 * You must provide a key name. The given key name will be removed from the classes data. If
	 * you provide a nonexistent key (case is important) then the function will return false.
	 * @return Returns true on success.
	 */
	bool Shrink(const std::string &key);
	
	/** Get an extension item.
	 *
	 * @param key The key parameter is an arbitary string which identifies the extension data
	 * @param p If you provide a non-existent key, this value will be NULL. Otherwise a pointer to the item you requested will be placed in this templated parameter.
	 * @return Returns true if the item was found and false if it was nor, regardless of wether 'p' is NULL. This allows you to store NULL values in Extensible.
	 */
	template<typename T> bool GetExt(const std::string &key, T* &p)
	{
		ExtensibleStore::iterator iter = this->Extension_Items.find(key); /* Find the item */
		if(iter != this->Extension_Items.end())
		{
			p = (T*)iter->second;	/* Item found */
			return true;
		}
		else
		{
			p = NULL;		/* Item not found */
			return false;
		}
	}
	
	/** Get an extension item.
	 *
	 * @param key The key parameter is an arbitary string which identifies the extension data
	 * @return Returns true if the item was found and false if it was not.
	 * 
	 * This single-parameter version only checks if the key exists, it does nothing with
	 * the 'data' field and is probably only useful in conjunction with the single-parameter
	 * version of Extend().
	 */
	bool GetExt(const std::string &key)
	{
		return (this->Extension_Items.find(key) != this->Extension_Items.end());
	}

	/** Get a list of all extension items names.
	 * @param list A deque of strings to receive the list
	 * @return This function writes a list of all extension items stored in this object by name into the given deque and returns void.
	 */
	void GetExtList(std::deque<std::string> &list);
};

/** BoolSet is a utility class designed to hold eight bools in a bitmask.
 * Use BoolSet::Set and BoolSet::Get to set and get bools in the bitmask,
 * and Unset and Invert for special operations upon them.
 */
class CoreExport BoolSet : public classbase
{
	/** Actual bit values */
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

/** This class can be used on its own to represent an exception, or derived to represent a module-specific exception.
 * When a module whishes to abort, e.g. within a constructor, it should throw an exception using ModuleException or
 * a class derived from ModuleException. If a module throws an exception during its constructor, the module will not
 * be loaded. If this happens, the error message returned by ModuleException::GetReason will be displayed to the user
 * attempting to load the module, or dumped to the console if the ircd is currently loading for the first time.
 */
class CoreExport CoreException : public std::exception
{
 protected:
	/** Holds the error message to be displayed
	 */
	const std::string err;
	/** Source of the exception
	 */
	const std::string source;
 public:
	/** Default constructor, just uses the error mesage 'Core threw an exception'.
	 */
	CoreException() : err("Core threw an exception"), source("The core") {}
	/** This constructor can be used to specify an error message before throwing.
	 */
	CoreException(const std::string &message) : err(message), source("The core") {}
	/** This constructor can be used to specify an error message before throwing,
	 * and to specify the source of the exception.
	 */
	CoreException(const std::string &message, const std::string &src) : err(message), source(src) {}
	/** This destructor solves world hunger, cancels the world debt, and causes the world to end.
	 * Actually no, it does nothing. Never mind.
	 * @throws Nothing!
	 */
	virtual ~CoreException() throw() {};
	/** Returns the reason for the exception.
	 * The module should probably put something informative here as the user will see this upon failure.
	 */
	virtual const char* GetReason()
	{
		return err.c_str();
	}

	virtual const char* GetSource()
	{
		return source.c_str();
	}
};

class CoreExport ModuleException : public CoreException
{
 public:
	/** Default constructor, just uses the error mesage 'Module threw an exception'.
	 */
	ModuleException() : CoreException("Module threw an exception", "A Module") {}

	/** This constructor can be used to specify an error message before throwing.
	 */
	ModuleException(const std::string &message) : CoreException(message, "A Module") {}
	/** This destructor solves world hunger, cancels the world debt, and causes the world to end.
	 * Actually no, it does nothing. Never mind.
	 * @throws Nothing!
	 */
	virtual ~ModuleException() throw() {};
};

#endif
