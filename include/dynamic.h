/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2019-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Oliver Lupton <om@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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


#pragma once

/** The extension that dynamic libraries end with. */
#if defined __APPLE__
# define DLL_EXTENSION ".dylib"
#elif defined  _WIN32
# define DLL_EXTENSION ".dll"
#else
# define DLL_EXTENSION ".so"
#endif

/** The DLLManager class is able to load a module file by filename,
 * and locate its init_module symbol.
 */
class CoreExport DLLManager final
	: public Cullable
{
private:
	/** The last error string. */
	std::string err;

	/** The module library handle. */
#ifdef _WIN32
	HMODULE lib = static_cast<HMODULE>(INVALID_HANDLE_VALUE);
#else
	void* lib = nullptr;
#endif

	/** The filename of the module library. */
	const std::string libname;

	/** Sets the last error string. */
	void RetrieveLastError();

public:
	/** Attempts to load the specified module.
	 * @param name The name of the library to load.
	 */
	DLLManager(const std::string& name);

	/** Unloads the module if one was loaded. */
	~DLLManager() override;

	/** Attempts to create a new module instance from this shared library.
	 * @return Either a new instance of the Module class or NULL on error.
	 */
	Module* CallInit();

	/** Retrieves the value of the specified symbol.
	 * @param name The name of the symbol to retrieve.
	 * @return Either the value of the specified symbol or or NULL if it does not exist.
	 */
	void* GetSymbol(const char* name) const;

	/** Retrieves the value of the specified symbol and casts it to the requested type.
	 * @param name The name of the symbol to retrieve.
	 * @return Either the value of the specified symbol or or NULL if it does not exist.
	 */
	template <typename TReturn>
	TReturn* GetSymbol(const char* name) const
	{
		return static_cast<TReturn*>(GetSymbol(name));
	}

	/** Retrieves the module version from the dynamic library. */
	const char* GetVersion() const { return GetSymbol<const char>(MODULE_STR_VERSION); }

	/** Retrieves the last error which occurred or an empty string if no errors have occurred. */
	const std::string& LastError() const { return err; }

	/** Retrieves the filename of the underlying shared library. */
	const std::string& LibraryName() const { return libname; }
};
