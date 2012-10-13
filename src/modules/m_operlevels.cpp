/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005 Craig Edwards <craigedwards@brainbox.cc>
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


/* $ModDesc: Gives each oper type a 'level', cannot kill opers 'above' your level. */

#include "inspircd.h"

class ModuleOperLevels : public Module
{
	public:
		void init()
		{
			ServerInstance->Modules->Attach(I_OnKill, this);
		}

		virtual Version GetVersion()
		{
			return Version("Gives each oper type a 'level', cannot kill opers 'above' your level.", VF_VENDOR);
		}

		virtual ModResult OnKill(User* source, User* dest, const std::string &reason)
		{
			// oper killing an oper?
			if (IS_OPER(dest) && IS_OPER(source))
			{
				std::string level = dest->oper->getConfig("level");
				long dest_level = atol(level.c_str());
				level = source->oper->getConfig("level");
				long source_level = atol(level.c_str());

				if (dest_level > source_level)
				{
					if (IS_LOCAL(source)) ServerInstance->SNO->WriteGlobalSno('a', "Oper %s (level %ld) attempted to /kill a higher oper: %s (level %ld): Reason: %s",source->nick.c_str(),source_level,dest->nick.c_str(),dest_level,reason.c_str());
					dest->WriteServ("NOTICE %s :*** Oper %s attempted to /kill you!",dest->nick.c_str(),source->nick.c_str());
					source->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - Oper %s is a higher level than you",source->nick.c_str(),dest->nick.c_str());
					return MOD_RES_DENY;
				}
			}
			return MOD_RES_PASSTHRU;
		}
};

MODULE_INIT(ModuleOperLevels)

