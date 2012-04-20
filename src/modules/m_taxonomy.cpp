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


#include "inspircd.h"

/* $ModDesc: Provides the /TAXONOMY command, used to view all metadata attached to a user */

/** Handle /TAXONOMY
 */
class CommandTaxonomy : public Command
{
	Module* Creator;
	bool& claimed;
 public:
	/* Command 'taxonomy', takes no parameters and needs no special modes */
	CommandTaxonomy (InspIRCd* Instance, Module* maker, bool &claim) : Command(Instance,"TAXONOMY", "o", 1), Creator(maker), claimed(claim)
	{
		this->source = "m_taxonomy.so";
		syntax = "<nickname>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
		{
			std::deque<std::string> list;
			dest->GetExtList(list);
			user->WriteNumeric(304, "" + std::string(user->nick) + ":TAXONOMY ITEMS " + std::string(dest->nick) + " " +ConvToStr(list.size()));
			for (unsigned int j = 0; j < list.size(); j++)
			{
				claimed = false;
				FOREACH_MOD(I_OnSyncUserMetaData, OnSyncUserMetaData(dest, Creator, user, list[j], true));
				if (!claimed)
				{
					user->WriteNumeric(304, "" + std::string(user->nick) + ":TAXONOMY METADATA " + list[j] + " = <unknown>");
				}
			}
			user->WriteNumeric(304, "" + std::string(user->nick) + ":TAXONOMY END");
		}
		return CMD_LOCALONLY;
	}
};

class ModuleTaxonomy : public Module
{
	CommandTaxonomy* newcommand;
	bool claimed;
 public:
	ModuleTaxonomy(InspIRCd* Me)
		: Module(Me)
	{

		// Create a new command
		newcommand = new CommandTaxonomy(ServerInstance, this, claimed);
		ServerInstance->AddCommand(newcommand);
		Implementation eventlist[] = { I_ProtoSendMetaData };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	void ProtoSendMetaData(void* opaque, TargetTypeFlags target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		if (target_type == TYPE_USER)
		{
			User* spoolto = (User*)opaque;
			std::string taxstr = "304 " + std::string(spoolto->nick) + ":TAXONOMY METADATA "+extname+" = "+extdata;
			spoolto->WriteServ(taxstr);
			claimed = true;
		}
	}

	virtual ~ModuleTaxonomy()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleTaxonomy)
