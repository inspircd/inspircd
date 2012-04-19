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


#ifndef SASL_H
#define SASL_H

class SASLHook : public classbase
{
 public:
	SASLHook() {}
	virtual ~SASLHook() {}
	virtual void ProcessClient(User* user, const std::vector<std::string>& parameters) = 0;
	virtual void ProcessServer(User* user, const std::vector<std::string>& parameters) = 0;
};

class SASLSearch : public Event
{
 public:
	User* const user;
	const std::string method;
	SASLHook* auth;
	SASLSearch(Module* me, User* u, const std::string& m)
		: Event(me, "sasl_search"), user(u), method(m), auth(NULL)
	{
		Send();
	}
};

#endif
