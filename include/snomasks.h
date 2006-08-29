/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * --------------------------------------------------
 */

#ifndef __SNOMASKS_H__
#define __SNOMASKS_H__

#include <string>
#include <vector>
#include "configreader.h"
#include "inspircd.h"

class SnomaskManager : public Extensible
{
 private:
	InspIRCd* ServerInstance;
 public:
	SnomaskManager(InspIRCd* Instance);
	~SnomaskManager();


};

#endif
