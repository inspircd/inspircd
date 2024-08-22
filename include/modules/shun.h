/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2021, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
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


#pragma once

#include "xline.h"

/** Shun class
 */
class Shun final
	: public XLine
{
public:
	/** Create a Shun.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 * @param shunmask Mask to match
	 */
	Shun(time_t s_time, unsigned long d, const std::string& src, const std::string& re, const std::string& shunmask)
		: XLine(s_time, d, src, re, "SHUN")
		, matchtext(shunmask)
	{
	}

	bool Matches(User* u) const override
	{
		LocalUser* lu = IS_LOCAL(u);
		if (lu && lu->exempt)
			return false;

		// Try to match nick!duser@dhost or nick!ruser@rhost
		if (InspIRCd::Match(u->GetMask(), matchtext) || InspIRCd::Match(u->GetRealMask(), matchtext))
			return true;

		// Try to match nick!ruser@address
		const std::string addressmask = FMT::format("{}!{}", u->nick, u->GetUserAddress());
		if (InspIRCd::Match(addressmask, matchtext))
			return true;

		// Try to match address
		if (InspIRCd::MatchCIDR(u->GetAddress(), matchtext, ascii_case_insensitive_map))
			return true;

		return false;
	}

	bool Matches(const std::string& str) const override
	{
		return (matchtext == str);
	}

	const std::string& Displayable() const override
	{
		return matchtext;
	}

private:
	/** Matching mask **/
	std::string matchtext;
};
