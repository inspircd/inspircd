/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014, 2016 Attila Molnar <attilamolnar@hush.com>
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
#include "translate.h"

CommandBase::Params Translate::ModeChangeListToParams(const Modes::ChangeList::List& modes)
{
	CommandBase::Params ret;
	ret.reserve(modes.size());

	for (const auto& item : modes)
	{
		ModeHandler* mh = item.mh;
		if (!mh->NeedsParam(item.adding))
			continue;

		if (mh->IsPrefixMode())
		{
			auto* target = ServerInstance->Users.Find(item.param);
			if (target)
			{
				ret.push_back(target->uuid);
				continue;
			}
		}

		ret.push_back(item.param);
	}
	return ret;
}

CommandBase::Params Translate::ParamsToNetwork(const std::vector<TranslateType>& types, const CommandBase::Params& params, CommandBase* custom_translator)
{
	CommandBase::Params newparams;
	newparams.reserve(params.size());

	auto typeit = types.begin();
	for (size_t index = 0; index < params.size(); ++index)
	{
		auto type = TranslateType::TEXT;

		// If less translation types than parameters are specified then we assume that all remaining
		// types are TranslateType::TEXT
		if (typeit != types.end())
		{
			type = *typeit;
			typeit++;
		}

		newparams.push_back(SingleParamToNetwork(type, params[index], custom_translator, index));
	}

	return newparams;
}

std::string Translate::SingleParamToNetwork(TranslateType type, const std::string& item, CommandBase* custom_translator, size_t index)
{
	switch (type)
	{
		case TranslateType::NICK: // Translate a nickname to a UUID.
		{
			auto* user = ServerInstance->Users.Find(item);
			if (user)
				return user->uuid;
			break;
		}
		case TranslateType::CUSTOM:
		{
			if (custom_translator)
				return custom_translator->EncodeParameter(item, index);
			break;
		}

		default:
			break;
	}

	return item;
}
