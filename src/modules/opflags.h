/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __OPFLAGS_H__
#define __OPFLAGS_H__

class OpFlagProvider : public DataProvider
{
 public:
	OpFlagProvider(Module* mod, const std::string& Name) : DataProvider(mod, Name) {}

	/** Get the list of flags for a user; NULL if empty */
	virtual std::string GetFlags(Membership* memb) = 0;

	/**
	 * Check if the user has permission based on a mode/flag list
	 * @param memb The user/channel to check
	 * @param needed The string of modes to check. In the format [level]{,flag}*
	 * @return True to allow, false to deny
	 *
	 * Example: "op,speaker,moderator" will allow anyone with op or higher,
	 * in addition to anyone who has the "speaker" or "moderator" flags. You can
	 * also do "*,speaker" to only permit those with the "speaker" flag.
	 */
	virtual bool PermissionCheck(Membership*, const std::string& needed) = 0;
};

#endif

