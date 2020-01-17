/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2019 Sadie Powell <sadie@witchery.services>
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

/** The DLLManager class is able to load a module file by filename,
 * and locate its init_module symbol.
 */
class CoreExport DLLManager : public classbase
{
 protected:
	/** The last error string
	 */
	std::string err;

	/** Sets the last error string
	*/
	void RetrieveLastError();

 public:
	/** This constructor loads the module using dlopen()
	 * @param fname The filename to load. This should be within
	 * the modules dir.
	 */
	DLLManager(const char *fname);
	virtual ~DLLManager();

	/** Get the last error from dlopen() or dlsym().
	 */
	const std::string& LastError()
	{
		 return err;
	}

	/** The module library handle.
	 */
	void *h;

	/** Return a module by calling the init function
	 */
	Module* CallInit();

	/** Retrieves the value of the specified symbol.
	 * @param name The name of the symbol to retrieve.
	 * @return Either the value of the specified symbol or or NULL if it does not exist.
	 */
	void* GetSymbol(const char* name);

	/** Get detailed version information from the module file */
	std::string GetVersion();
};
