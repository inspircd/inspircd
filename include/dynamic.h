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


#ifndef __DLL_H
#define __DLL_H

/** This typedef represents the init_module function within each module.
 * The init_module function is the only exported extern "C" declaration
 * in any module file.
 */
typedef void * (initfunc) (void);

#include "inspircd_config.h"

class InspIRCd;

/** The DLLManager class is able to load a module file by filename,
 * and locate its init_module symbol.
 */
class DLLManager
{
 public:
	/** This constructor loads the module using dlopen()
	 * @param ServerInstance The creator class of this object
	 * @param fname The filename to load. This should be within
	 * the modules dir.
	 */
	DLLManager(InspIRCd* ServerInstance, const char *fname);
	virtual ~DLLManager();


#ifdef STATIC_LINK
	/** Get a symbol using static linking.
	 * @param v A static function pointer, pointing at an init_module function
	 * @param sym_name The symbol name to find, usually "init_module"
	 * @return True if the symbol can be found
	 */
	bool GetSymbol(initfunc* &v, const char *sym_name);
#else
	/** Get a symbol using dynamic linking.
	 * @param v A function pointer, pointing at an init_module function
	 * @param sym_name The symbol name to find, usually "init_module"
	 * @return true if the symbol can be found, also the symbol will be put into v.
	 */
	bool GetSymbol(void **v, const char *sym_name);
#endif
	/** Get the last error from dlopen() or dlsym().
	 * @return The last error string, or NULL if no error has occured
	 */
	char* LastError() 
	{
		 return err;
	}

	/** The module handle
	 */
	void *h;

 protected:

	/** The last error string, or NULL
	 */
	char *err;
#ifdef STATIC_LINK

	/** The module name
	 */
	char staticname[1024];
#endif
};

/** This class is a specialized form of DLLManager designed to load InspIRCd modules.
 * It's job is to call the init_module function and receive a factory pointer.
 */
class DLLFactoryBase : public DLLManager
{
 public:
	/** Default constructor.
	 * This constructor loads a module file by calling its DLLManager subclass constructor,
	 * then finds the symbol using DLLManager::GetSymbol(), and calls the symbol,
	 * obtaining a valid pointer to the init_module function
	 */
	DLLFactoryBase(InspIRCd* Instance, const char *fname, const char *func_name = 0);

	/** Default destructor
	 */
	virtual ~DLLFactoryBase();
#ifdef STATIC_LINK
	/** A function pointer to the factory function
	 */
	initfunc *factory_func;
#else
	/** A function pointer to the factory function
	 */
	void * (*factory_func)(void);	
#endif
};

/** This is the highest-level class of the DLLFactory system used to load InspIRCd modules.
 * Its job is to finally call the init_module function and obtain a pointer to a ModuleFactory.
 * This template is a container for ModuleFactory itself, so that it may 'plug' into ModuleFactory
 * and provide module loading capabilities transparently.
 */
template <class T> class DLLFactory : public DLLFactoryBase
{
 public:
	/** Default constructor.
	 * This constructor passes its paramerers down through DLLFactoryBase and then DLLManager
	 * to load the module, then calls the factory function to retrieve a pointer to a ModuleFactory
	 * class. It is then down to the core to call the ModuleFactory::CreateModule() method and
	 * receive a Module* which it can insert into its module lists.
	 */
	DLLFactory(InspIRCd* Instance, const char *fname, const char *func_name=0) : DLLFactoryBase(Instance, fname, func_name)
	{
		if (factory_func)
			factory = reinterpret_cast<T*>(factory_func());
		else
			factory = reinterpret_cast<T*>(-1);
	}
	
	/** The destructor deletes the ModuleFactory pointer.
	 */
	~DLLFactory()
	{
		if (factory)
			delete factory;
	}

	/** The ModuleFactory pointer.
	 */
	T *factory;
};

#endif
