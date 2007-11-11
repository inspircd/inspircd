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

#include "inspircd.h"

/* $ModDesc: A dummy module for testing */

// Class ModuleRemoteInclude inherits from Module
// It just outputs simple debug strings to show its methods are working.

class ModuleRemoteInclude : public Module
{
 private:
	 
 public:
	ModuleRemoteInclude(InspIRCd* Me)
		: Module(Me)
	{
		ServerInstance->Modules->Attach(I_OnDownloadFile, this);
	}
	
	virtual ~ModuleRemoteInclude()
	{
	}
	
	virtual Version GetVersion()
	{
		// this method instantiates a class of type Version, and returns
		// the modules version information using it.
	
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}

	int OnDownloadFile(const std::string &name, std::istream* &filedata)
	{
		/* Dummy code */
		std::stringstream* ss = new std::stringstream();
		(*ss) << "<test tag="">";

		delete filedata;
		filedata = ss;

		/* for this test module, we claim all schemes, and we return dummy data.
		 * Because the loading is instant we mark the file completed immediately.
		 */
		ServerInstance->Config->Complete(name, false);

		return true;
	}
};


MODULE_INIT(ModuleRemoteInclude)

