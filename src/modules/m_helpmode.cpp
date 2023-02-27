/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2023 Sadie Powell <sadie@witchery.services>
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
#include "modules/whois.h"

class ModuleHelpMode final
	: public Module
	, public Whois::EventListener
{
private:
	SimpleUserMode helpop;

public:
	ModuleHelpMode()
		: Module(VF_VENDOR, "Adds user mode h (helpop) which marks a server operator as being available for help.")
		, Whois::EventListener(this)
		, helpop(this, "helpop", 'h', true)
	{
	}

	void OnWhois(Whois::Context& whois) override
	{
		if (whois.GetTarget()->IsModeSet(helpop))
			whois.SendLine(RPL_WHOISHELPOP, "is available for help.");
	}
};

MODULE_INIT(ModuleHelpMode)
