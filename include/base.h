/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __BASE_H__
#define __BASE_H__

#include <list>
#include <map>
#include <deque>
#include <string>

/** Dummy class to help enforce culls being parent-called up to classbase */
class CullResult
{
	CullResult();
	friend class classbase;
};

/** The base class for all inspircd classes with a well-defined lifetime.
 * Classes that inherit from this may be destroyed through GlobalCulls,
 * and may rely on cull() being called prior to their deletion.
 */
class CoreExport classbase
{
 public:
	classbase();

	/**
	 * Called just prior to destruction via cull list.
	 */
	virtual CullResult cull();
	virtual ~classbase();
 private:
	// uncopyable
	classbase(const classbase&);
	void operator=(const classbase&);
};

/** The base class for inspircd classes that provide a wrapping interface, and
 * should only exist while being used. Prevents heap allocation.
 */
class CoreExport interfacebase
{
 public:
	interfacebase() {}
 private:
	interfacebase(const interfacebase&);
	void operator=(const interfacebase&);
	void* operator new(size_t);
	void operator delete(void*);
};

/** The base class for inspircd classes that support reference counting.
 * Any objects that do not have a well-defined lifetime should inherit from
 * this, and should be assigned to a reference<type> object to establish their
 * lifetime.
 *
 * Reference objects should not hold circular references back to themselves,
 * even indirectly; this will cause a memory leak because the count will never
 * drop to zero.
 *
 * Using a normal pointer for the object is recommended if you can assure that
 * at least one reference<> will remain as long as that pointer is used; this
 * will avoid the slight overhead of changing the reference count.
 */
class CoreExport refcountbase
{
	mutable unsigned int refcount;
 public:
	refcountbase();
	virtual ~refcountbase();
	inline unsigned int GetReferenceCount() const { return refcount; }
	void* operator new(size_t);
	void operator delete(void*);
	inline void refcount_inc() const { refcount++; }
	inline bool refcount_dec() const { refcount--; return !refcount; }
 private:
	// uncopyable
	refcountbase(const refcountbase&);
	void operator=(const refcountbase&);
};

/** Base class for use count tracking. Uses reference<>, but does not
 * cause object deletion when the last user is removed.
 *
 * Safe for use as a second parent class; will not add a second vtable.
 */
class CoreExport usecountbase
{
	mutable unsigned int usecount;
 public:
	usecountbase() : usecount(0) { }
	~usecountbase();
	inline unsigned int GetUseCount() const { return usecount; }
	inline void refcount_inc() const { usecount++; }
	inline bool refcount_dec() const { usecount--; return false; }
 private:
	// uncopyable
	usecountbase(const usecountbase&);
	void operator=(const usecountbase&);
};

template <typename T>
class reference
{
	T* value;
 public:
	reference() : value(0) { }
	reference(T* v) : value(v) { if (value) value->refcount_inc(); }
	reference(const reference<T>& v) : value(v.value) { if (value) value->refcount_inc(); }
	reference<T>& operator=(const reference<T>& other)
	{
		if (other.value)
			other.value->refcount_inc();
		this->reference::~reference();
		value = other.value;
		return *this;
	}

	~reference()
	{
		if (value && value->refcount_dec())
			delete value;
	}
	inline operator bool() const { return value; }
	inline operator T*() const { return value; }
	inline T* operator->() const { return value; }
	inline T& operator*() const { return *value; }
	inline bool operator<(const reference<T>& other) const { return value < other.value; }
	inline bool operator>(const reference<T>& other) const { return value > other.value; }
 private:
	void* operator new(size_t);
	void operator delete(void*);
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
	/** This constructor can be used to specify an error message before throwing.
	 */
	ModuleException(const std::string &message, Module* me = NULL);
};

typedef const reference<Module> ModuleRef;

enum ServiceType {
	/** is a Command */
	SERVICE_COMMAND,
	/** is a ModeHandler */
	SERVICE_MODE,
	/** is a metadata descriptor */
	SERVICE_METADATA,
	/** is a data processing provider (MD5, SQL) */
	SERVICE_DATA,
	/** is an I/O hook provider (SSL) */
	SERVICE_IOHOOK
};

/** A structure defining something that a module can provide */
class CoreExport ServiceProvider : public classbase
{
 public:
	/** Module that is providing this service */
	ModuleRef creator;
	/** Name of the service being provided */
	const std::string name;
	/** Type of service (must match object type) */
	const ServiceType service;
	ServiceProvider(Module* Creator, const std::string& Name, ServiceType Type)
		: creator(Creator), name(Name), service(Type) {}
	virtual ~ServiceProvider();
};


#endif
