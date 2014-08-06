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

class CustomPrefixMode : public PrefixMode
{
 public:
	reference<ConfigTag> tag;
	bool depriv;

	CustomPrefixMode(Module* parent, ConfigTag* Tag)
		: PrefixMode(parent, Tag->getString("name"), 0, Tag->getInt("rank"))
		, tag(Tag)
	{
		std::string v = tag->getString("prefix");
		prefix = v.c_str()[0];
		v = tag->getString("letter");
		mode = v.c_str()[0];
		levelrequired = tag->getInt("ranktoset", prefixrank);
		depriv = tag->getBool("depriv", true);
	}

	ModResult AccessCheck(User* src, Channel*, std::string& value, bool adding)
	{
		if (!adding && src->nick == value && depriv)
			return MOD_RES_ALLOW;
		return MOD_RES_PASSTHRU;
	}
};

class ModuleCustomPrefix : public Module
{
	std::vector<CustomPrefixMode*> modes;
 public:
	void init() CXX11_OVERRIDE
	{
		ConfigTagList tags = ServerInstance->Config->ConfTags("customprefix");
		while (tags.first != tags.second)
		{
			ConfigTag* tag = tags.first->second;
			tags.first++;
			CustomPrefixMode* mh = new CustomPrefixMode(this, tag);
			modes.push_back(mh);
			if (mh->GetPrefixRank() == 0)
				throw ModuleException("Rank must be specified for prefix at " + tag->getTagLocation());
			if (!isalpha(mh->GetModeChar()))
				throw ModuleException("Mode must be a letter for prefix at " + tag->getTagLocation());
			try
			{
				ServerInstance->Modules->AddService(*mh);
			}
			catch (ModuleException& e)
			{
				throw ModuleException(e.GetReason() + " (while creating mode from " + tag->getTagLocation() + ")");
			}
		}
	}

	~ModuleCustomPrefix()
	{
		stdalgo::delete_all(modes);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides custom prefix channel modes", VF_VENDOR);
	}
};

MODULE_INIT(ModuleCustomPrefix)
