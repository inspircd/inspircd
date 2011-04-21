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

	/** Get the list of flags for a user */
	virtual std::string GetFlags(Membership* memb) = 0;

	/** Set the flag list for a user */
	virtual void SetFlags(Membership* memb, const std::string& flags, bool sendGlobal) = 0;
	virtual std::string SetFlags(Membership* memb, const std::set<std::string>& flags, bool sendGlobal) = 0;

	/**
	 * Check if the user has permission based on a mode/flag list
	 * @param memb The user/channel to check
	 * @param needed A list of flags to check for
	 * @return true if the user has at least one of the listed flags
	 */
	virtual bool PermissionCheck(Membership*, const std::string& needed) = 0;
};

class OpFlagPermissionData : public PermissionData
{
 public:
	std::string& delta;
	OpFlagPermissionData(User* src, Channel* c, User* u, std::string& Delta)
		: PermissionData(src, "opflags", c, u, false), delta(Delta) {}
};

#endif

