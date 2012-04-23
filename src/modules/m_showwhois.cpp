/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2005, 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2004 Christopher Hall <typobox43@gmail.com>
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

/* $ModDesc: Allows opers to set +W to see when a user uses WHOIS on them */

/** Handle user mode +W
 */
class SeeWhois : public ModeHandler
{
 public:
	SeeWhois(InspIRCd* Instance, bool IsOpersOnly) : ModeHandler(Instance, 'W', 0, 0, false, MODETYPE_USER, IsOpersOnly) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		if (adding)
		{
			if (!dest->IsModeSet('W'))
			{
				dest->SetMode('W',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('W'))
			{
				dest->SetMode('W',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleShowwhois : public Module
{
	bool ShowWhoisFromOpers;
	SeeWhois* sw;

 public:

	ModuleShowwhois(InspIRCd* Me) : Module(Me)
	{
		ConfigReader conf(ServerInstance);
		bool OpersOnly = conf.ReadFlag("showwhois", "opersonly", "yes", 0);
		ShowWhoisFromOpers = conf.ReadFlag("showwhois", "showfromopers", "yes", 0);

		sw = new SeeWhois(ServerInstance, OpersOnly);
		if (!ServerInstance->Modes->AddMode(sw))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnWhois };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	~ModuleShowwhois()
	{
		ServerInstance->Modes->DelMode(sw);
		delete sw;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_COMMON|VF_VENDOR,API_VERSION);
	}

	virtual void OnWhois(User* source, User* dest)
	{
		if ((dest->IsModeSet('W')) && (source != dest))
		{
			if (!ShowWhoisFromOpers && IS_OPER(source))
				return;

			std::string wmsg = "*** ";
			wmsg += source->nick + " (" + source->ident + "@";

			/* XXX HasPrivPermission doesn't work correctly for remote users */
			if (IS_LOCAL(dest) && dest->HasPrivPermission("users/auspex"))
			{
				wmsg += source->host;
			}
			else
			{
				wmsg += source->dhost;
			}

			wmsg += ") did a /whois on you";

			if (IS_LOCAL(dest))
			{
				dest->WriteServ("NOTICE %s :%s", dest->nick.c_str(), wmsg.c_str());
			}
			else
			{
				std::string msg = std::string("::") + dest->server + " NOTICE " + dest->nick + " :" + wmsg;
				ServerInstance->PI->PushToClient(dest, msg);
			}
		}
	}

};

MODULE_INIT(ModuleShowwhois)
