/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "modules.h"
#define OVRREQID "Override Request"
class OVRrequest : public Request
{
public:
User * requser;
std::string reqtoken;
OVRrequest(Module* s, Module* d, User* src, const std::string &token)
        : Request(s, d, OVRREQID), reqtoken(token)
	{
		requser = src;
	}
};
