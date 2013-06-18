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
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "builtinmodes.h"

ModeUserServerNoticeMask::ModeUserServerNoticeMask() : ModeHandler(NULL, "snomask", 's', PARAM_SETONLY, MODETYPE_USER)
{
	oper = true;
}

ModeAction ModeUserServerNoticeMask::OnModeChange(User* source, User* dest, Channel*, std::string &parameter, bool adding)
{
	/* Set the array fields */
	if (adding)
	{
		/* Fix for bug #310 reported by Smartys */
		if (!dest->IsModeSet(this))
			dest->snomasks.reset();

		dest->SetMode(this, true);
		parameter = ProcessNoticeMasks(dest, parameter.c_str());
		return MODEACTION_ALLOW;
	}
	else
	{
		if (dest->IsModeSet(this))
		{
			dest->SetMode(this, false);
			return MODEACTION_ALLOW;
		}
	}

	/* Allow the change */
	return MODEACTION_DENY;
}

std::string ModeUserServerNoticeMask::GetUserParameter(User* user)
{
	std::string masks = FormatNoticeMasks(user);
	if (masks.length())
		masks = "+" + masks;
	return masks;
}

void ModeUserServerNoticeMask::OnParameterMissing(User* user, User* dest, Channel* channel)
{
	user->WriteNotice("*** The user mode +s requires a parameter (server notice mask). Please provide a parameter, e.g. '+s +*'.");
}

std::string ModeUserServerNoticeMask::ProcessNoticeMasks(User* user, const char *sm)
{
	bool adding = true, oldadding = false;
	const char *c = sm;
	std::string output;

	while (c && *c)
	{
		switch (*c)
		{
			case '+':
				adding = true;
			break;
			case '-':
				adding = false;
			break;
			case '*':
				for (unsigned char d = 'a'; d <= 'z'; d++)
				{
					if (!ServerInstance->SNO->masks[d - 'a'].Description.empty())
					{
						if ((!user->IsNoticeMaskSet(d) && adding) || (user->IsNoticeMaskSet(d) && !adding))
						{
							if ((oldadding != adding) || (!output.length()))
								output += (adding ? '+' : '-');

							SetNoticeMask(user, d, adding);

							output += d;
						}
						oldadding = adding;
						char u = toupper(d);
						if ((!user->IsNoticeMaskSet(u) && adding) || (user->IsNoticeMaskSet(u) && !adding))
						{
							if ((oldadding != adding) || (!output.length()))
								output += (adding ? '+' : '-');

							SetNoticeMask(user, u, adding);

							output += u;
						}
						oldadding = adding;
					}
				}
			break;
			default:
				if (isalpha(*c))
				{
					if ((!user->IsNoticeMaskSet(*c) && adding) || (user->IsNoticeMaskSet(*c) && !adding))
					{
						if ((oldadding != adding) || (!output.length()))
							output += (adding ? '+' : '-');

						SetNoticeMask(user, *c, adding);

						output += *c;
					}
				}
				else
					user->WriteNumeric(ERR_UNKNOWNSNOMASK, "%s %c :is unknown snomask char to me", user->nick.c_str(), *c);

				oldadding = adding;
			break;
		}

		c++;
	}

	std::string s = this->FormatNoticeMasks(user);
	if (s.length() == 0)
	{
		user->SetMode(this, false);
	}

	return output;
}

std::string ModeUserServerNoticeMask::FormatNoticeMasks(User* user)
{
	std::string data;

	for (unsigned char n = 0; n < 64; n++)
	{
		if (user->snomasks[n])
			data.push_back(n + 65);
	}

	return data;
}

void ModeUserServerNoticeMask::SetNoticeMask(User* user, unsigned char sm, bool value)
{
	if (!isalpha(sm))
		return;
	user->snomasks[sm-65] = value;
}
