/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021, 2024 Sadie Powell <sadie@witchery.services>
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


#include "inspircd.h"
#include "main.h"

namespace
{
	size_t NextToken(const std::string& line, size_t start)
	{
		if (start == std::string::npos || start + 1 > line.length())
			return std::string::npos;

		return line.find(' ', start + 1);
	}
}

bool TreeSocket::PreProcessNewProtocolMessage(User*& who, std::string& cmd, CommandBase::Params& params)
{
	if (proto_version == PROTO_INSPIRCD_4)
	{
		if (insp::casemapped_equals(cmd, "ENCAP"))
		{
			if (params.size() < 2)
				return false; // Malformed.

			// :<uuid> ENCAP <target> <command> [<params>...];
			CommandBase::Params newparams(params.begin() + 2, params.end());
			if (!PreProcessNewProtocolMessage(who, params[1], newparams))
				return false; // Malformed.

			params.erase(params.begin() + 2, params.end());
			params.insert(params.end(), newparams.begin(), newparams.end());
		}
		else if (insp::casemapped_equals(cmd, "FAIL") || insp::casemapped_equals(cmd, "WARN") || insp::casemapped_equals(cmd, "NOTE"))
		{
			// <source-sid> <target-uuid> <command> <code> [<params>...] :<message>
			if (params.size() < 5)
				return false; // Malformed.

			// InspIRCd v5 introduced networking of standard replies. For v4 and earlier fall back
			// to a notice from the server.
			cmd = "NOTICE";

			const auto* server = Utils->FindServerID(params[0]);
			who = server ? server->ServerUser : this->MyRoot->ServerUser;

			const auto has_command = params[2] != "*";
			CommandBase::Params newparams;
			newparams.push_back(params[1]);
			newparams.push_back(FMT::format("*** {}{}{}",
				has_command ? params[2] : "",
				has_command ? ": " : "",
				params.back()
			));

			std::swap(params, newparams);
		}
		else if (insp::casemapped_equals(cmd, "FJOIN"))
		{
			// :<sid> FJOIN <chan> <chants> <modes> :[<modes>],<uuid>:<membid>/<joined> [<modes>],<uuid>:<membid>/<joined>
			//                                                                ^^^^^^^^^ New in 1207
			if (params.size() < 4)
				return false; // Malformed

			size_t pos = 0;
			while (pos != std::string::npos)
			{
				auto next = NextToken(params[3], pos);
				auto slash = params[3].find('/', pos);
				if (slash != std::string::npos)
					params[3].erase(slash, next - slash);
				pos = next;
			}
		}
		else if (insp::casemapped_equals(cmd, "IJOIN"))
		{
			if (params.size() < 3)
				return false; // Malformed.

			// :<uuid> IJOIN <chan> <membid> <joints> [<chants> <modes>]
			//                               ^^^^^^^^ New in 1207
			params.erase(params.begin() + 2);
		}
		else if (insp::casemapped_equals(cmd, "SETHOST") || insp::casemapped_equals(cmd, "SETIDENT") || insp::casemapped_equals(cmd, "SETNAME"))
		{
				// CHG* was merged with SET* in v5. Rewrite to the old commands.
			cmd.replace(0, 3, "CHG");
		}
	}
	return true;
}

bool TreeSocket::PreProcessOldProtocolMessage(User*& who, std::string& cmd, CommandBase::Params& params)
{
	if (insp::casemapped_equals(cmd, "CHGHOST") || insp::casemapped_equals(cmd, "CHGIDENT") || insp::casemapped_equals(cmd, "CHGNAME"))
	{
		if (params.size() < 2)
			return false; // Malformed.

		// CHG* and SET* were merged in v5.
		cmd.replace(0, 3, "SET");
	}
	else if (insp::casemapped_equals(cmd, "ENCAP"))
	{
		if (params.size() < 2)
			return false; // Malformed.

		// :<uuid> ENCAP <target> <command> [<params>...];
		CommandBase::Params newparams(params.begin() + 2, params.end());
		if (!PreProcessOldProtocolMessage(who, params[1], newparams))
			return false; // Malformed.

		params.erase(params.begin() + 2, params.end());
		params.insert(params.end(), newparams.begin(), newparams.end());
	}
	else if (insp::casemapped_equals(cmd, "IJOIN"))
	{
		if (params.size() < 3)
			return false; // Malformed.

		// :<uuid> IJOIN <chan> <membid> <joints> [<chants> <modes>]
		//                               ^^^^^^^^ New in 1207
		params.insert(params.begin() + 2, "0");
	}
	return true;
}
