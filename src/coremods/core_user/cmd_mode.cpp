/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2008 Craig Edwards <craigedwards@brainbox.cc>
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "inspircd.h"
#include "core_user.h"

CommandMode::CommandMode(Module* parent)
	: Command(parent, "MODE", 1)
	, seq(0)
{
	syntax = "<target> [[(+|-)]<modes> [<mode-parameters>]]";
	memset(&sent, 0, sizeof(sent));
}

CmdResult CommandMode::Handle(User* user, const Params& parameters)
{
	const std::string& target = parameters[0];
	Channel* targetchannel = ServerInstance->FindChan(target);
	User* targetuser = NULL;
	if (!targetchannel)
	{
		if (IS_LOCAL(user))
			targetuser = ServerInstance->FindNickOnly(target);
		else
			targetuser = ServerInstance->FindNick(target);
	}

	if ((!targetchannel) && (!targetuser))
	{
		if (target[0] == '#')
			user->WriteNumeric(Numerics::NoSuchChannel(target));
		else
			user->WriteNumeric(Numerics::NoSuchNick(target));
		return CMD_FAILURE;
	}
	if (parameters.size() == 1)
	{
		this->DisplayCurrentModes(user, targetuser, targetchannel);
		return CMD_SUCCESS;
	}

	// Populate a temporary Modes::ChangeList with the parameters
	Modes::ChangeList changelist;
	ModeType type = targetchannel ? MODETYPE_CHANNEL : MODETYPE_USER;
	ServerInstance->Modes.ModeParamsToChangeList(user, type, parameters, changelist);

	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnPreMode, MOD_RESULT, (user, targetuser, targetchannel, changelist));

	ModeParser::ModeProcessFlag flags = ModeParser::MODE_NONE;
	if (IS_LOCAL(user))
	{
		if (MOD_RESULT == MOD_RES_PASSTHRU)
		{
			if ((targetuser) && (user != targetuser))
			{
				// Local users may only change the modes of other users if a module explicitly allows it
				user->WriteNumeric(ERR_USERSDONTMATCH, "Can't change mode for other users");
				return CMD_FAILURE;
			}

			// This is a mode change by a local user and modules didn't explicitly allow/deny.
			// Ensure access checks will happen for each mode being changed.
			flags |= ModeParser::MODE_CHECKACCESS;
		}
		else if (MOD_RESULT == MOD_RES_DENY)
			return CMD_FAILURE; // Entire mode change denied by a module
	}
	else
		flags |= ModeParser::MODE_LOCALONLY;

	if (IS_LOCAL(user))
		ServerInstance->Modes->ProcessSingle(user, targetchannel, targetuser, changelist, flags);
	else
		ServerInstance->Modes->Process(user, targetchannel, targetuser, changelist, flags);

	if ((ServerInstance->Modes.GetLastChangeList().empty()) && (targetchannel) && (parameters.size() == 2))
	{
		/* Special case for displaying the list for listmodes,
		 * e.g. MODE #chan b, or MODE #chan +b without a parameter
		 */
		this->DisplayListModes(user, targetchannel, parameters[1]);
	}

	return CMD_SUCCESS;
}

RouteDescriptor CommandMode::GetRouting(User* user, const Params& parameters)
{
	return (IS_LOCAL(user) ? ROUTE_LOCALONLY : ROUTE_BROADCAST);
}

void CommandMode::DisplayListModes(User* user, Channel* chan, const std::string& mode_sequence)
{
	seq++;

	for (std::string::const_iterator i = mode_sequence.begin(); i != mode_sequence.end(); ++i)
	{
		unsigned char mletter = *i;
		if (mletter == '+')
			continue;

		ModeHandler* mh = ServerInstance->Modes->FindMode(mletter, MODETYPE_CHANNEL);
		if (!mh || !mh->IsListMode())
			return;

		/* Ensure the user doesnt request the same mode twice,
		 * so they can't flood themselves off out of idiocy.
		 */
		if (sent[mletter] == seq)
			continue;

		sent[mletter] = seq;
		ServerInstance->Modes.ShowListModeList(user, chan, mh);
	}
}

static std::string GetSnomasks(const User* user)
{
	ModeHandler* const snomask = ServerInstance->Modes.FindMode('s', MODETYPE_USER);
	std::string snomaskstr = snomask->GetUserParameter(user);
	// snomaskstr is empty if the snomask mode isn't set, otherwise it begins with a '+'.
	// In the former case output a "+", not an empty string.
	if (snomaskstr.empty())
		snomaskstr.push_back('+');
	return snomaskstr;
}

namespace
{
	void GetModeList(Numeric::Numeric& num, Channel* chan, User* user)
	{
		// We should only show the value of secret parameters (i.e. key) if
		// the user is a member of the channel.
		bool show_secret = chan->HasUser(user);

		size_t modepos = num.push("+").GetParams().size() - 1;
		std::string modes;
		std::string param;
		for (unsigned char chr = 65; chr < 123; ++chr)
		{
			// Check that the mode exists and is set.
			ModeHandler* mh = ServerInstance->Modes->FindMode(chr, MODETYPE_CHANNEL);
			if (!mh || !chan->IsModeSet(mh))
				continue;

			// Add the mode to the set list.
			modes.push_back(mh->GetModeChar());

			// If the mode has a parameter we need to include that too.
			ParamModeBase* pm = mh->IsParameterMode();
			if (!pm)
				continue;

			// If a mode has a secret parameter and the user is not privy to
			// the value of it then we use <name> instead of the value.
			if (pm->IsParameterSecret() && !show_secret)
			{
				num.push("<" + pm->name + ">");
				continue;
			}

			// Retrieve the parameter and add it to the mode list.
			pm->GetParameter(chan, param);
			num.push(param);
			param.clear();
		}
		num.GetParams()[modepos].append(modes);
	}
}

void CommandMode::DisplayCurrentModes(User* user, User* targetuser, Channel* targetchannel)
{
	if (targetchannel)
	{
		// Display channel's current mode string
		Numeric::Numeric modenum(RPL_CHANNELMODEIS);
		modenum.push(targetchannel->name);
		GetModeList(modenum, targetchannel, user);
		user->WriteNumeric(modenum);
		user->WriteNumeric(RPL_CHANNELCREATED, targetchannel->name, (unsigned long)targetchannel->age);
	}
	else
	{
		if (targetuser == user)
		{
			// Display user's current mode string
			user->WriteNumeric(RPL_UMODEIS, targetuser->GetModeLetters());
			if (targetuser->IsOper())
				user->WriteNumeric(RPL_SNOMASKIS, GetSnomasks(targetuser), "Server notice mask");
		}
		else if (user->HasPrivPermission("users/auspex"))
		{
			// Querying the modes of another user.
			// We cannot use RPL_UMODEIS because that's only for showing the user's own modes.
			user->WriteNumeric(RPL_OTHERUMODEIS, targetuser->nick, targetuser->GetModeLetters());
			if (targetuser->IsOper())
				user->WriteNumeric(RPL_OTHERSNOMASKIS, targetuser->nick, GetSnomasks(targetuser), "Server notice mask");
		}
		else
		{
			user->WriteNumeric(ERR_USERSDONTMATCH, "Can't view modes for other users");
		}
	}
}
