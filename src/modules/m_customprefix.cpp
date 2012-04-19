/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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

/* $ModDesc: Allows custom prefix modes to be created. */

class CustomPrefixMode : public PrefixModeHandler
{
 public:
	reference<ConfigTag> tag;
	int rank;
	bool depriv;
	CustomPrefixMode(Module* parent, ConfigTag* Tag)
		: PrefixModeHandler(parent, Tag->getString("name"), 0), tag(Tag)
	{
		std::string v = tag->getString("prefix");
		prefix = v.c_str()[0];
		v = tag->getString("letter");
		mode = v.c_str()[0];
		rank = tag->getInt("rank");
		levelrequired = tag->getInt("ranktoset", rank);
		depriv = tag->getBool("depriv", true);
	}

	unsigned int GetPrefixRank()
	{
		return rank;
	}

	void AccessCheck(ModePermissionData& perm)
	{
		if (!perm.mc.adding && perm.source == perm.user && depriv)
			perm.result = MOD_RES_ALLOW;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		return MODEACTION_ALLOW;
	}
};

class ModuleCustomPrefix : public Module
{
	std::vector<CustomPrefixMode*> modes;
 public:
	ModuleCustomPrefix()
	{
	}

	void init()
	{
		ConfigTagList tags = ServerInstance->Config->GetTags("customprefix");
		while (tags.first != tags.second)
		{
			ConfigTag* tag = tags.first->second;
			tags.first++;
			CustomPrefixMode* mh = new CustomPrefixMode(this, tag);
			modes.push_back(mh);
			if (mh->rank <= 0)
				throw ModuleException("Rank must be specified for prefix in " + tag->getTagLocation());
			try
			{
				ServerInstance->Modules->AddService(*mh);
			}
			catch (ModuleException& e)
			{
				throw ModuleException(e.err + " (while creating mode from " + tag->getTagLocation() + ")");
			}
		}
	}

	~ModuleCustomPrefix()
	{
		for (std::vector<CustomPrefixMode*>::iterator i = modes.begin(); i != modes.end(); i++)
			delete *i;
	}

	Version GetVersion()
	{
		return Version("Provides custom prefix channel modes", VF_VENDOR);
	}
};

MODULE_INIT(ModuleCustomPrefix)
