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

std::string Translate::ModeChangeListToParams(const Modes::ChangeList::List& modes)
{
	std::string ret;
	for (const auto& item : modes)
	{
		ModeHandler* mh = item.mh;
		if (!mh->NeedsParam(item.adding))
			continue;

		ret.push_back(' ');

		if (mh->IsPrefixMode())
		{
			auto* target = ServerInstance->Users.Find(item.param);
			if (target)
			{
				ret.append(target->uuid);
				continue;
			}
		}

		ret.append(item.param);
	}
	return ret;
}
