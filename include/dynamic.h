/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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


#ifndef DLL_H
#define DLL_H

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

	/** Get detailed version information from the module file */
	std::string GetVersion();
};

#endif

