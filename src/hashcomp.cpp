/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2013, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007 Craig Edwards <brain@inspircd.org>
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

irc::portparser::portparser(const std::string& source, bool allow_overlapped)
	: sep(source, ',')
	, overlapped(allow_overlapped)
{
}

bool irc::portparser::Overlaps(long val)
{
	if (overlapped)
		return false;

	return (!overlap_set.insert(val).second);
}

long irc::portparser::GetToken()
{
	if (in_range > 0)
	{
		in_range++;
		if (in_range <= range_end)
		{
			if (!Overlaps(in_range))
			{
				return in_range;
			}
			else
			{
				while (((Overlaps(in_range)) && (in_range <= range_end)))
					in_range++;

				if (in_range <= range_end)
					return in_range;
			}
		}
		else
			in_range = 0;
	}

	std::string x;
	sep.GetToken(x);

	if (x.empty())
		return 0;

	while (Overlaps(ConvToNum<long>(x)))
	{
		if (!sep.GetToken(x))
			return 0;
	}

	std::string::size_type dash = x.rfind('-');
	if (dash != std::string::npos)
	{
		std::string sbegin(x, 0, dash);
		range_begin =  ConvToNum<long>(sbegin);
		range_end =  ConvToNum<long>(x.c_str() + dash + 1);

		if ((range_begin > 0) && (range_end > 0) && (range_begin < 65536) && (range_end < 65536) && (range_begin < range_end))
		{
			in_range = range_begin;
			return in_range;
		}
		else
		{
			/* Assume its just the one port */
			return ConvToNum<long>(sbegin);
		}
	}
	else
	{
		return ConvToNum<long>(x);
	}
}
