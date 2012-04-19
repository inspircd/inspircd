/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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


#ifndef OPFLAGS_H
#define OPFLAGS_H

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

