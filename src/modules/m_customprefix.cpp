/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
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
	std::shared_ptr<ConfigTag> tag;

	CustomPrefixMode(Module* parent, const std::string& Name, char Letter, char Prefix, std::shared_ptr<ConfigTag> Tag)
		: PrefixMode(parent, Name, Letter, 0, Prefix)
		, tag(Tag)
	{
		unsigned int rank = static_cast<unsigned int>(tag->getUInt("rank", 0, 0, UINT_MAX));
		unsigned int setrank = static_cast<unsigned int>(tag->getUInt("ranktoset", prefixrank, rank, UINT_MAX));
		unsigned int unsetrank = static_cast<unsigned int>(tag->getUInt("ranktounset", setrank, setrank, UINT_MAX));
		bool depriv = tag->getBool("depriv", true);
		this->Update(rank, setrank, unsetrank, depriv);

		ServerInstance->Logs.Log(MODNAME, LOG_DEBUG, "Created the %s prefix: letter=%c prefix=%c rank=%u ranktoset=%u ranktounset=%i depriv=%d",
			name.c_str(), GetModeChar(), GetPrefix(), GetPrefixRank(), GetLevelRequired(true), GetLevelRequired(false), CanSelfRemove());
	}
};

class ModuleCustomPrefix : public Module
{
 private:
	std::vector<CustomPrefixMode*> modes;

 public:
	ModuleCustomPrefix()
		: Module(VF_VENDOR, "Allows the server administrator to configure custom channel prefix modes.")
	{
	}

	void init() override
	{
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("customprefix"))
		{
			const std::string name = tag->getString("name");
			if (name.empty())
				throw ModuleException("<customprefix:name> must be specified at " + tag->source.str());

			if (tag->getBool("change"))
			{
				ModeHandler* mh = ServerInstance->Modes.FindMode(name, MODETYPE_CHANNEL);
				if (!mh)
					throw ModuleException("<customprefix:change> specified for a nonexistent mode at " + tag->source.str());

				PrefixMode* pm = mh->IsPrefixMode();
				if (!pm)
					throw ModuleException("<customprefix:change> specified for a non-prefix mode at " + tag->source.str());

				unsigned int rank = static_cast<unsigned int>(tag->getUInt("rank", pm->GetPrefixRank(), 0, UINT_MAX));
				unsigned int setrank = static_cast<unsigned int>(tag->getUInt("ranktoset", pm->GetLevelRequired(true), rank, UINT_MAX));
				unsigned int unsetrank = static_cast<unsigned int>(tag->getUInt("ranktounset", pm->GetLevelRequired(false), setrank, UINT_MAX));
				bool depriv = tag->getBool("depriv", pm->CanSelfRemove());
				pm->Update(rank, setrank, unsetrank, depriv);

				ServerInstance->Logs.Log(MODNAME, LOG_DEBUG, "Changed the %s prefix: depriv=%u rank=%u ranktoset=%u ranktounset=%u",
					pm->name.c_str(), pm->CanSelfRemove(), pm->GetPrefixRank(), pm->GetLevelRequired(true), pm->GetLevelRequired(false));
				continue;
			}

			const std::string letter = tag->getString("letter");
			if (letter.length() != 1)
				throw ModuleException("<customprefix:letter> must be set to a mode character at " + tag->source.str());

			const std::string prefix = tag->getString("prefix");
			if (prefix.length() != 1)
				throw ModuleException("<customprefix:prefix> must be set to a mode prefix at " + tag->source.str());

			try
			{
				CustomPrefixMode* mh = new CustomPrefixMode(this, name, letter[0], prefix[0], tag);
				modes.push_back(mh);
				ServerInstance->Modules.AddService(*mh);
			}
			catch (ModuleException& e)
			{
				throw ModuleException(e.GetReason() + " (while creating mode from " + tag->source.str() + ")");
			}
		}
	}

	~ModuleCustomPrefix() override
	{
		stdalgo::delete_all(modes);
	}
};

MODULE_INIT(ModuleCustomPrefix)
