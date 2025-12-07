/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2020-2023, 2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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

class Link final
{
public:
	std::shared_ptr<ConfigTag> tag;
	std::string Name;
	std::string IPAddr;
	in_port_t Port;
	std::string SendPass;
	std::string RecvPass;
	std::string Fingerprint;
	std::vector<std::string> AllowMasks;
	bool HiddenFromStats;
	std::string Hook;
	unsigned long Timeout;
	std::string Bind;
	bool Hidden;
	int Protocol = 0;
	Link(const std::shared_ptr<ConfigTag>& Tag)
		: tag(Tag)
	{
	}
};

class Autoconnect final
{
public:
	std::shared_ptr<ConfigTag> tag;
	std::vector<std::string> servers;
	unsigned long Period;
	unsigned long BootPeriod;
	time_t NextConnectTime;
	/** Negative == inactive */
	int position;
	Autoconnect(const std::shared_ptr<ConfigTag>& Tag)
		: tag(Tag)
	{
	}
};
