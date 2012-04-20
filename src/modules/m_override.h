/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
