/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __USERMANAGER_H
#define __USERMANAGER_H

class CoreExport UserManager : public classbase
{
 private:
	InspIRCd *ServerInstance;
 public:
	UserManager(InspIRCd *Instance)
	{
		ServerInstance = Instance;
	}
};

#endif
