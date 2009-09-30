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

	/**
	 * Called just prior to destruction via cull list.
	 *
	 * @return true to allow the delete, or false to halt the delete
	 */
	virtual bool cull();
	virtual ~classbase();
};

/** The base class for inspircd classes that support reference counting.
 * Any objects that do not have a well-defined lifetime should inherit from
 * this
 */
class CoreExport refcountbase : public classbase
{
	unsigned int refcount;
 public:
	refcountbase();
	virtual bool cull();
	virtual ~refcountbase();
	inline unsigned int GetReferenceCount() const { return refcount; }
	friend class reference_base;
};

class CoreExport reference_base
{
 protected:
	static inline unsigned int inc(refcountbase* v) { return ++(v->refcount); }
	static inline unsigned int dec(refcountbase* v) { return --(v->refcount); }
};

template <typename T>
class CoreExport reference : public reference_base
{
	T* value;
 public:
	reference() : value(0) { }
	reference(T* v) : value(v) { if (value) inc(value); }
	reference(const reference& v) : value(v.value) { if (value) inc(value); }
	reference<T>& operator=(const reference<T>& other)
	{
		if (other.value)
			inc(other.value);
		this->reference::~reference();
		value = other.value;
		return *this;
	}

	~reference()
	{
		if (value)
		{
			int rc = dec(value);
			if (rc == 0 && value->cull())
				delete value;
		}
	}
	inline const T* operator->() const { return value; }
	inline const T& operator*() const { return *value; }
	inline T* operator->() { return value; }
	inline T& operator*() { return *value; }
	operator bool() const { return value; }
	operator T*() const { return value; }
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
