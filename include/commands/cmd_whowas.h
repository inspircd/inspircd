/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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


#ifndef CMD_WHOWAS_H
#define CMD_WHOWAS_H

class WhoWasMaintainer : public DataProvider
{
 public:
	WhoWasMaintainer(Module* mod) : DataProvider(mod, "whowas_maintain") {}
	virtual void AddToWhoWas(User* user) = 0;
	virtual std::string GetStats() = 0;
	virtual void PruneWhoWas(time_t t) = 0;
	virtual void MaintainWhoWas(time_t t) = 0;
};

#endif
