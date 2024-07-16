/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
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

class CustomPrefixMode final
	: public PrefixMode
{
public:
	std::shared_ptr<ConfigTag> tag;

	CustomPrefixMode(Module* parent, const std::string& Name, char Letter, char Prefix, const std::shared_ptr<ConfigTag>& Tag)
		: PrefixMode(parent, Name, Letter, 0, Prefix)
		, tag(Tag)
	{
		ModeHandler::Rank rank = tag->getNum<ModeHandler::Rank>("rank", 1, 1);
		ModeHandler::Rank setrank = tag->getNum<ModeHandler::Rank>("ranktoset", prefixrank, rank);
		ModeHandler::Rank unsetrank = tag->getNum<ModeHandler::Rank>("ranktounset", setrank, setrank);
		bool depriv = tag->getBool("depriv", true);
		this->Update(rank, setrank, unsetrank, depriv);

		ServerInstance->Logs.Debug(MODNAME, "Created the {} prefix: letter={} prefix={} rank={} ranktoset={} ranktounset={} depriv={}",
			name, GetModeChar(), GetPrefix(), GetPrefixRank(), GetLevelRequired(true), GetLevelRequired(false),
			CanSelfRemove() ? "yes" : "no");
	}
};

class ModuleCustomPrefix final
	: public Module
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
				throw ModuleException(this, "<customprefix:name> must be specified at " + tag->source.str());

			if (name.find(' ') != std::string::npos)
				throw ModuleException(this, "<customprefix:name> must not contain spaces at " + tag->source.str());

			if (tag->getBool("change"))
			{
				ModeHandler* mh = ServerInstance->Modes.FindMode(name, MODETYPE_CHANNEL);
				if (!mh)
					throw ModuleException(this, "<customprefix:change> specified for a nonexistent mode at " + tag->source.str());

				PrefixMode* pm = mh->IsPrefixMode();
				if (!pm)
					throw ModuleException(this, "<customprefix:change> specified for a non-prefix mode at " + tag->source.str());

				ModeHandler::Rank rank = tag->getNum<ModeHandler::Rank>("rank", pm->GetPrefixRank(), 1);
				ModeHandler::Rank setrank = tag->getNum<ModeHandler::Rank>("ranktoset", pm->GetLevelRequired(true), rank);
				ModeHandler::Rank unsetrank = tag->getNum<ModeHandler::Rank>("ranktounset", pm->GetLevelRequired(false), setrank);
				bool depriv = tag->getBool("depriv", pm->CanSelfRemove());
				pm->Update(rank, setrank, unsetrank, depriv);

				ServerInstance->Logs.Debug(MODNAME, "Changed the {} prefix: letter={} prefix={} rank={} ranktoset={} ranktounset={} depriv={}",
					name, pm->GetModeChar(), pm->GetPrefix(), pm->GetPrefixRank(), pm->GetLevelRequired(true),
					pm->GetLevelRequired(false), pm->CanSelfRemove() ? "yes" : "no");
				continue;
			}

			const std::string letter = tag->getString("letter");
			if (letter.length() != 1)
				throw ModuleException(this, "<customprefix:letter> must be set to a mode character at " + tag->source.str());

			const std::string prefix = tag->getString("prefix");
			if (prefix.length() != 1)
				throw ModuleException(this, "<customprefix:prefix> must be set to a mode prefix at " + tag->source.str());

			try
			{
				auto* mh = new CustomPrefixMode(this, name, letter[0], prefix[0], tag);
				modes.push_back(mh);
				ServerInstance->Modules.AddService(*mh);
			}
			catch (const ModuleException& e)
			{
				throw ModuleException(this, e.GetReason() + " (while creating mode from " + tag->source.str() + ")");
			}
		}
	}

	~ModuleCustomPrefix() override
	{
		stdalgo::delete_all(modes);
	}
};

MODULE_INIT(ModuleCustomPrefix)
