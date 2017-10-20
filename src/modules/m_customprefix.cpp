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

	CustomPrefixMode(Module* parent, const std::string& Name, char Letter, char Prefix, ConfigTag* Tag)
		: PrefixMode(parent, Name, Letter, 0, Prefix)
		, tag(Tag)
	{
		prefixrank = tag->getInt("rank", 0, 0, UINT_MAX);
		ranktoset = tag->getInt("ranktoset", prefixrank, prefixrank, UINT_MAX);
		ranktounset = tag->getInt("ranktounset", ranktoset, ranktoset, UINT_MAX);
		selfremove = tag->getBool("depriv", true);
	}
};

class ModuleCustomPrefix : public Module
{
	std::vector<CustomPrefixMode*> modes;
 public:
	void init() CXX11_OVERRIDE
	{
		ConfigTagList tags = ServerInstance->Config->ConfTags("customprefix");
		for (ConfigIter iter = tags.first; iter != tags.second; ++iter)
		{
			ConfigTag* tag = iter->second;

			const std::string name = tag->getString("name");
			if (name.empty())
				throw ModuleException("<customprefix:name> must be specified at " + tag->getTagLocation());

			const std::string letter = tag->getString("letter");
			if (letter.length() != 1)
				throw ModuleException("<customprefix:letter> must be set to a mode character at " + tag->getTagLocation());

			const std::string prefix = tag->getString("prefix");
			if (prefix.length() != 1)
				throw ModuleException("<customprefix:prefix> must be set to a mode prefix at " + tag->getTagLocation());

			try
			{
				CustomPrefixMode* mh = new CustomPrefixMode(this, name, letter[0], prefix[0], tag);
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
