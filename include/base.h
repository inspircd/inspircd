/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __BASE_H__
#define __BASE_H__

#include <map>
#include <deque>
#include <string>

/** The base class for all inspircd classes.
 * Wherever possible, all classes you create should inherit from this,
 * giving them the ability to be passed to various core functions
 * as 'anonymous' classes.
*/
class CoreExport classbase
{
 public:
	classbase();

	virtual ~classbase() { }
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
