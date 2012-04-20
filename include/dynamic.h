/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2003-2004, 2007 Craig Edwards <craigedwards@brainbox.cc>
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


#ifndef __DLL_H
#define __DLL_H

/** The DLLManager class is able to load a module file by filename,
 * and locate its init_module symbol.
 */
class CoreExport DLLManager
{
 protected:

	/** The last error string, or NULL
	 */
	const char *err;

 public:
	/** This constructor loads the module using dlopen()
	 * @param ServerInstance The creator class of this object
	 * @param fname The filename to load. This should be within
	 * the modules dir.
	 */
	DLLManager(InspIRCd* ServerInstance, const char *fname);
	virtual ~DLLManager();

	/** Get a symbol using dynamic linking.
	 * @param v A function pointer, pointing at an init_module function
	 * @param sym_name The symbol name to find, usually "init_module"
	 * @return true if the symbol can be found, also the symbol will be put into v.
	 */
	bool GetSymbol(void **v, const char *sym_name);

	/** Get the last error from dlopen() or dlsym().
	 * @return The last error string, or NULL if no error has occured.
	 */
	const char* LastError()
	{
		 return err;
	}

	/** The module handle.
	 * This is OS dependent, on POSIX platforms it is a pointer to a function
	 * pointer (yes, really!) and on windows it is a library handle.
	 */
	void *h;
};

class CoreExport LoadModuleException : public CoreException
{
 public:
	/** This constructor can be used to specify an error message before throwing.
	 */
	LoadModuleException(const std::string &message)
	: CoreException(message, "the core")
	{
	}

	/** This destructor solves world hunger, cancels the world debt, and causes the world to end.
	 * Actually no, it does nothing. Never mind.
	 * @throws Nothing!
	 */
	virtual ~LoadModuleException() throw() {};
};

class CoreExport FindSymbolException : public CoreException
{
 public:
	/** This constructor can be used to specify an error message before throwing.
	 */
	FindSymbolException(const std::string &message)
	: CoreException(message, "the core")
	{
	}

	/** This destructor solves world hunger, cancels the world debt, and causes the world to end.
	 * Actually no, it does nothing. Never mind.
	 * @throws Nothing!
	 */
	virtual ~FindSymbolException() throw() {};
};

/** This is the highest-level class of the DLLFactory system used to load InspIRCd modules and commands.
 * All the dirty mucking around with dl*() is done by DLLManager, all this does it put a pretty shell on
 * it and make it nice to use to load modules and core commands. This class is quite specialised for these
 * two uses and it may not be useful more generally -- use DLLManager directly for that.
 */
template <typename ReturnType> class CoreExport DLLFactory : public DLLManager
{
 protected:
	/** This typedef represents the init_* function within each module or command.
	 * The init_module function is the only exported extern "C" declaration
	 * in any module file. In a cmd_*.cpp file the equivilant is init_command
	 */
	typedef ReturnType * (initfunctype) (InspIRCd*);

	/** Pointer to the init function.
	 */
	initfunctype* init_func;

	/** Instance pointer to be passed to init_*() when it is called.
	 */
	InspIRCd* ServerInstance;

 public:
	/** Default constructor.
	 * This constructor passes its paramerers down through DLLFactoryBase and then DLLManager
	 * to load the module, then calls the factory function to retrieve a pointer to a ModuleFactory
	 * class. It is then down to the core to call the ModuleFactory::CreateModule() method and
	 * receive a Module* which it can insert into its module lists.
	 */
	DLLFactory(InspIRCd* Instance, const char *fname, const char *func_name)
	: DLLManager(Instance, fname), init_func(NULL), ServerInstance(Instance)
	{
		const char* error = LastError();

		if(!error)
		{
			if(!GetSymbol((void **)&init_func, func_name))
			{
				throw FindSymbolException("Missing " + std::string(func_name) + "() entrypoint!");
			}
		}
		else
		{
			throw LoadModuleException(error);
		}
	}

	/** Calls the 'init_module' C exported function within a module, which
	 * returns a pointer to a Module derived object.
	 */
	ReturnType* CallInit()
	{
		if(init_func)
		{
			return init_func(ServerInstance);
		}
		else
		{
			return NULL;
		}
	}

	/** The destructor deletes the ModuleFactory pointer.
	 */
	~DLLFactory()
	{
	}
};

#endif

