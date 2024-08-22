/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2017, 2020-2021, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Robin Burchell <robin+git@viroteck.net>
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
#include "core_oper.h"

ModeUserServerNoticeMask::ModeUserServerNoticeMask(Module* Creator)
	: ModeHandler(Creator, "snomask", 's', PARAM_SETONLY, MODETYPE_USER)
{
	oper = true;
	syntax = "(+|-)<snomasks>|*";
}

bool ModeUserServerNoticeMask::OnModeChange(User* source, User* dest, Channel*, Modes::Change& change)
{
	if (change.adding)
	{
		dest->SetMode(this, true);
		// Process the parameter (remove chars we don't understand, remove redundant chars, etc.)
		change.param = ProcessNoticeMasks(dest, change.param);
		return true;
	}
	else
	{
		if (dest->IsModeSet(this))
		{
			dest->SetMode(this, false);
			dest->snomasks.reset();
			return true;
		}
	}

	// Mode not set and trying to unset, deny
	return false;
}

std::string ModeUserServerNoticeMask::GetUserParameter(const User* user) const
{
	std::string ret;
	if (!user->IsModeSet(this))
		return ret;

	ret.push_back('+');
	for (unsigned char n = 0; n < 64; n++)
	{
		if (user->snomasks[n])
			ret.push_back(n + 'A');
	}
	return ret;
}

std::string ModeUserServerNoticeMask::ProcessNoticeMasks(User* user, const std::string& input)
{
	bool adding = true;
	std::bitset<64> curr = user->snomasks;

	for (const auto snomask : input)
	{
		switch (snomask)
		{
			case '+':
				adding = true;
			break;
			case '-':
				adding = false;
			break;
			case '*':
				for (size_t j = 0; j < 64; j++)
				{
					const char chr = j + 'A';
					if (user->HasSnomaskPermission(chr) && ServerInstance->SNO.IsSnomaskUsable(chr))
						curr[j] = adding;
				}
			break;
			default:
				// For local users check whether the given snomask is valid and enabled - IsSnomaskUsable() tests both.
				// For remote users accept what we were told, unless the snomask char is not a letter.
				if (IS_LOCAL(user))
				{
					if (!ServerInstance->SNO.IsSnomaskUsable(snomask))
					{
						user->WriteNumeric(ERR_UNKNOWNSNOMASK, snomask, "is an unknown snomask character");
						continue;
					}
					else if (!user->IsOper())
					{
						user->WriteNumeric(ERR_NOPRIVILEGES, FMT::format("Permission Denied - Only operators may {} snomask {}",
							adding ? "set" : "unset", snomask));
						continue;

					}
					else if (!user->HasSnomaskPermission(snomask))
					{
						user->WriteNumeric(ERR_NOPRIVILEGES, FMT::format("Permission Denied - Oper type {} does not have access to snomask {}",
							user->oper->GetType(), snomask));
						continue;
					}
				}
				else if (!((snomask >= 'a' && snomask <= 'z') || (snomask >= 'A' && snomask <= 'Z')))
					continue;

				size_t index = (snomask - 'A');
				curr[index] = adding;
			break;
		}
	}

	std::string plus = "+";
	std::string minus = "-";

	// Apply changes and construct two strings consisting of the newly added and the removed snomask chars
	for (size_t i = 0; i < 64; i++)
	{
		bool isset = curr[i];
		if (user->snomasks[i] != isset)
		{
			user->snomasks[i] = isset;
			std::string& appendhere = (isset ? plus : minus);
			appendhere.push_back(i+'A');
		}
	}

	// Create the final string that will be shown to the user and sent to servers
	// Form: "+ABc-de"
	std::string output;
	if (plus.length() > 1)
		output = std::move(plus);

	if (minus.length() > 1)
		output += minus;

	// Unset the snomask usermode itself if every snomask was unset
	if (user->snomasks.none())
		user->SetMode(this, false);

	return output;
}
