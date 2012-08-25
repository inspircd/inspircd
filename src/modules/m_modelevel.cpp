/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDesc: Tunes module levelrequired */

class ModuleModeLevel : public Module
{
 private:
 public:
	ModuleModeLevel() 	{
		Implementation eventlist[] = { I_OnRehash, I_OnLoadModule };
		ServerInstance->Modules->Attach(eventlist, this, 2);
		ResetLevels();
	}

	~ModuleModeLevel()
	{
	}

	void OnRehash(User* user)
	{
		ResetLevels();
	}

	void OnLoadModule(Module* module)
	{
		ResetLevels();
	}

	void ResetLevels()
	{
		ConfigTagList tags = ServerInstance->Config->ConfTags("modelevel");
		for(ConfigIter i=tags.first; i!=tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string letter = tag->getString("letter");
			ModeHandler* mh = ServerInstance->Modes->FindMode(letter[0], MODETYPE_CHANNEL);
			if(mh != NULL)
			{
				int level=tag->getPrefixValue("level");
				if(level>0)
				{
					mh->setLevelRequired(level);
					ServerInstance->Logs->Log("MODULE", DEBUG, "Changing modeletter %c to level %d",letter[0],level);
				}
			}
		}
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, PRIORITY_LAST);
	}

	Version GetVersion()
	{
		return Version("Module level requirement tuner",VF_OPTCOMMON);
	}


};


MODULE_INIT(ModuleModeLevel)

