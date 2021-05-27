/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

class Link
{
 public:
	std::shared_ptr<ConfigTag> tag;
	std::string Name;
	std::string IPAddr;
	unsigned int Port;
	std::string SendPass;
	std::string RecvPass;
	std::string Fingerprint;
	std::vector<std::string> AllowMasks;
	bool HiddenFromStats;
	std::string Hook;
	unsigned long Timeout;
	std::string Bind;
	bool Hidden;
	Link(std::shared_ptr<ConfigTag> Tag)
		: tag(Tag)
	{
	}
};

class Autoconnect
{
 public:
	std::shared_ptr<ConfigTag> tag;
	std::vector<std::string> servers;
	unsigned long Period;
	time_t NextConnectTime;
	/** Negative == inactive */
	int position;
	Autoconnect(std::shared_ptr<ConfigTag> Tag)
		: tag(Tag)
	{
	}
};
