/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Adam <Adam@anope.org>
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

ModeUserServerNoticeMask::ModeUserServerNoticeMask() : ModeHandler(NULL, "snomask", 's', PARAM_ALWAYS, MODETYPE_USER)
{
	oper = true;
}

ModeAction ModeUserServerNoticeMask::OnModeChange(User* source, User* dest, Channel*, std::string &parameter, bool adding)
{
	parameter = ProcessNoticeMasks(dest, parameter, adding);
	return MODEACTION_ALLOW;
}

std::string ModeUserServerNoticeMask::GetUserParameter(User* user)
{
	std::string ret;

	if (!user->IsModeSet(this))
		return ret;

	for (unsigned i = 0; i < user->snomasks.size(); ++i)
		if (user->snomasks[i])
		{
			bool remote = i & 1;
			Snomask *sno = SnomaskManager::FindSnomaskByPos(i - remote);
			if (sno == NULL)
				continue;

			std::string r = remote ? "REMOTE" : "";
			ret += "," + r + sno->name;
		}

	if (!ret.empty())
	{
		ret = ret.substr(1);
		std::transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
	}

	return ret;
}

void ModeUserServerNoticeMask::OnParameterMissing(User* user, User* dest, Channel* channel)
{
	user->WriteNotice("*** The user mode +" + ConvToStr(mode) + " requires a parameter (server notice mask). Please provide a parameter, e.g. '+" + ConvToStr(mode) + " +*'.");
}

std::string ModeUserServerNoticeMask::ProcessNoticeMasks(User* user, const std::string& input, bool adding)
{
	irc::commasepstream sep(input);
	std::set<std::string> changed;

	for (std::string token; sep.GetToken(token);)
	{
		if (token == "*")
		{
			std::vector<Snomask*> v = SnomaskManager::GetSnomasks();
			for (unsigned j = 0; j < v.size(); ++j)
			{
				Snomask *s = v[j];

				if (user->snomasks[s->pos] != adding)
				{
					user->snomasks[s->pos] = adding;
					changed.insert(s->name);
				}
				if (user->snomasks[s->pos + 1] != adding)
				{
					user->snomasks[s->pos + 1] = adding;
					changed.insert("REMOTE" + s->name);
				}
			}
			continue;
		}

		std::transform(token.begin(), token.end(), token.begin(), ::toupper);
		bool remote = false;
		if (!token.find("REMOTE"))
		{
			token = token.substr(6);
			remote = true;
		}
		Snomask* snomask = SnomaskManager::FindSnomaskByName(token);
		if (snomask == NULL)
		{
			if (IS_LOCAL(user))
				user->WriteNumeric(ERR_UNKNOWNSNOMASK, "%s :is unknown snomask to me", token.c_str());
			continue;
		}

		if (user->snomasks[snomask->pos + remote] != adding)
		{
			user->snomasks[snomask->pos + remote] = adding;
			changed.insert((remote ? "REMOTE" : "") + snomask->name);
		}
	}

	user->SetMode(this, !user->snomasks.none());

	return irc::stringjoiner(changed, ',');
}
