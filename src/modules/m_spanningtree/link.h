/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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


#ifndef M_SPANNINGTREE_LINK_H
#define M_SPANNINGTREE_LINK_H

class Link : public refcountbase
{
 public:
	reference<ConfigTag> tag;
	irc::string Name;
	std::string IPAddr;
	int Port;
	std::string SendPass;
	std::string RecvPass;
	std::string Fingerprint;
	std::string AllowMask;
	bool HiddenFromStats;
	std::string Hook;
	int Timeout;
	std::string Bind;
	bool Hidden;
	Link(ConfigTag* Tag) : tag(Tag) {}
};

class Autoconnect : public refcountbase
{
 public:
	reference<ConfigTag> tag;
	std::vector<std::string> servers;
	unsigned long Period;
	time_t NextConnectTime;
	/** Negative == inactive */
	int position;
	Autoconnect(ConfigTag* Tag) : tag(Tag) {}
};

#endif
