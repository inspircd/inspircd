/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2015 Justin Thomas Crawford <Justasic@gmail.com>
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

/* $ModDesc: Strip or entirely block color coded quit and part messages. */

class ModuleBlockColoredQuit : public Module
{

	// What do we block?
	bool BlockColoredQuit;
	bool StripColoredQuit;
	bool BlockColoredPart;
	bool StripColoredPart;

	// Module options
	bool NotifyUser;
	bool AffectOpers;

	bool HasColors(const std::string &text)
	{
			// borrowed from m_blockcolor.so
		for (std::string::const_iterator i = text.begin(); i != text.end(); i++)
		{
			switch (*i)
			{
				case 2:
				case 3:
				case 15:
				case 21:
				case 22:
				case 31:
					return true;
				break;
			}
		}
		return false;
	}

public:
	ModuleBlockColoredQuit()
	{
	}

	virtual ~ModuleBlockColoredQuit()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Strip or entirely block color coded quit and part messages.",VF_VENDOR);
	}

	virtual void init()
	{
		Implementation eventlist[] = { I_OnPreCommand, I_OnRehash };
        ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist) / sizeof(Implementation));
		this->OnRehash(NULL);
	}

 	virtual void OnRehash(User* user)
    {
        ConfigTag* tag = ServerInstance->Config->ConfValue("nocoloredquit");

		this->BlockColoredQuit = tag->getBool("blockcoloredquitmessages", true);
		this->StripColoredQuit = tag->getBool("stripcoloredquitmessages", false);
		this->BlockColoredPart = tag->getBool("blockcoloredpartmessages", false);
		this->StripColoredPart = tag->getBool("stripcoloredpartmessages", false);

		this->NotifyUser = tag->getBool("notifyusers", false);
		this->AffectOpers = tag->getBool("affectopers", false);
    }	

    virtual ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
	{
		if (command == "QUIT")
		{
			// If AffectOpers option is enabled, ignore oper quit messages entirely.
			if (IS_OPER(user) && !this->AffectOpers)
				return MOD_RES_PASSTHRU;

			bool hascolor = HasColors(parameters[0]);

			if (this->StripColoredQuit && hascolor)
			{
				InspIRCd::StripColor(parameters[0]);
			}
			else if (this->BlockColoredQuit && hascolor)
			{
				// Similar to m_shun.so
				parameters.clear();			
			}
		}

		if (command == "PART" && parameters.size() > 1)
		{
			// If AffectOpers option is enabled, ignore oper part messages entirely.
			if (IS_OPER(user) && !this->AffectOpers)
				return MOD_RES_PASSTHRU;

			bool hascolor = HasColors(parameters[1]);
						
			if (this->StripColoredPart && hascolor)
			{
				if (this->NotifyUser)
					user->WriteServ("NOTICE %s :*** Your part message from %s contained colors and has been stripped.", user->nick.c_str(), parameters[0].c_str());

				InspIRCd::StripColor(parameters[1]);
			}
			else if (this->BlockColoredPart && hascolor)
			{
				if (this->NotifyUser)
					user->WriteServ("NOTICE %s :*** Your part message from %s contained colors and the message has been removed.", user->nick.c_str(), parameters[0].c_str());

				parameters[1].clear();
			}
		}
		return MOD_RES_PASSTHRU;
	}
};


MODULE_INIT(ModuleBlockColoredQuit)
