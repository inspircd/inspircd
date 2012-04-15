/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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

