/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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
#include "builtinmodes.h"

ModeUserServerNoticeMask::ModeUserServerNoticeMask() : ModeHandler(NULL, "snomask", 's', PARAM_SETONLY, MODETYPE_USER)
{
	oper = true;
}

ModeAction ModeUserServerNoticeMask::OnModeChange(User* source, User* dest, Channel*, std::string &parameter, bool adding)
{
	if (adding)
	{
		dest->SetMode(this, true);
		// Process the parameter (remove chars we don't understand, remove redundant chars, etc.)
		parameter = ProcessNoticeMasks(dest, parameter);
		return MODEACTION_ALLOW;
	}
	else
	{
		if (dest->IsModeSet(this))
		{
			dest->SetMode(this, false);
			dest->snomasks.reset();
			return MODEACTION_ALLOW;
		}
	}

	// Mode not set and trying to unset, deny
	return MODEACTION_DENY;
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

void ModeUserServerNoticeMask::OnParameterMissing(User* user, User* dest, Channel* channel)
{
	user->WriteNotice("*** The user mode +s requires a parameter (server notice mask). Please provide a parameter, e.g. '+s +*'.");
}

std::string ModeUserServerNoticeMask::ProcessNoticeMasks(User* user, const std::string& input)
{
	bool adding = true;
	std::bitset<64> curr = user->snomasks;

	for (std::string::const_iterator i = input.begin(); i != input.end(); ++i)
	{
		switch (*i)
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
					if (ServerInstance->SNO->IsSnomaskUsable(j+'A'))
						curr[j] = adding;
				}
			break;
			default:
				// For local users check whether the given snomask is valid and enabled - IsSnomaskUsable() tests both.
				// For remote users accept what we were told, unless the snomask char is not a letter.
				if (IS_LOCAL(user))
				{
					if (!ServerInstance->SNO->IsSnomaskUsable(*i))
					{
						user->WriteNumeric(ERR_UNKNOWNSNOMASK, *i, "is unknown snomask char to me");
						continue;
					}
				}
				else if (!(((*i >= 'a') && (*i <= 'z')) || ((*i >= 'A') && (*i <= 'Z'))))
					continue;

				size_t index = ((*i) - 'A');
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
		output = plus;

	if (minus.length() > 1)
		output += minus;

	// Unset the snomask usermode itself if every snomask was unset
	if (user->snomasks.none())
		user->SetMode(this, false);

	return output;
}
