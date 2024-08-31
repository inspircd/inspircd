/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Andrio Celos <AndrioCelos@users.noreply.github.com>
 *   Copyright (C) 2021 Andrio Celos
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2014-2016 Attila Molnar <attilamolnar@hush.com>
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
#include "modules/isupport.h"
#include "numerichelper.h"

enum
{
	// From RFC 1459.
	RPL_UMODEIS = 221,
	RPL_CHANNELMODEIS = 324,
	ERR_USERSDONTMATCH = 502,

	// From ircu?
	RPL_CHANNELCREATED = 329,

	// From UnrealIRCd.
	RPL_SNOMASKIS = 8,

	// InspIRCd-specific.
	RPL_OTHERUMODEIS = 803,
	RPL_OTHERSNOMASKIS = 804,
};

class CommandMode final
	: public Command
{
private:
	unsigned int sent[256];
	unsigned int seq = 0;
	ChanModeReference secretmode;
	ChanModeReference privatemode;
	UserModeReference snomaskmode;

	/** Show the list of one or more list modes to a user.
	 * @param user User to send to.
	 * @param chan Channel whose lists to show.
	 * @param mode_sequence Mode letters to show the lists of.
	 */
	void DisplayListModes(User* user, Channel* chan, const std::string& mode_sequence);

	/** Show the current modes of a channel or a user to a user.
	 * @param user User to show the modes to.
	 * @param targetuser User whose modes to show. NULL if showing the modes of a channel.
	 * @param targetchannel Channel whose modes to show. NULL if showing the modes of a user.
	 */
	void DisplayCurrentModes(User* user, User* targetuser, Channel* targetchannel);

	bool CanSeeChan(User* user, Channel* chan)
	{
		// A user can always see the channel modes if they are:
		// (1) In the channel.
		// (2) An oper with the channels/auspex privilege.
		if (chan->HasUser(user) ||  user->HasPrivPermission("channels/auspex"))
			return true;

		// Otherwise, they can only see the modes when the channel is not +p or +s.
		return !chan->IsModeSet(secretmode) && !chan->IsModeSet(privatemode);
	}

	std::string GetSnomasks(const User* user);

public:
	CommandMode(Module* parent);
	CmdResult Handle(User* user, const Params& parameters) override;
	RouteDescriptor GetRouting(User* user, const Params& parameters) override;
};

CommandMode::CommandMode(Module* parent)
	: Command(parent, "MODE", 1)
	, secretmode(creator, "secret")
	, privatemode(creator, "private")
	, snomaskmode(creator, "snomask")
{
	syntax = { "<target> [[(+|-)]<modes> [<mode-parameters>]]" };
	memset(&sent, 0, sizeof(sent));
}

CmdResult CommandMode::Handle(User* user, const Params& parameters)
{
	const std::string& target = parameters[0];
	auto* targetchannel = ServerInstance->Channels.Find(target);
	User* targetuser = nullptr;
	if (!targetchannel)
	{
		if (IS_LOCAL(user))
			targetuser = ServerInstance->Users.FindNick(target);
		else
			targetuser = ServerInstance->Users.Find(target);
	}

	if ((!targetchannel || !CanSeeChan(user, targetchannel)) && (!targetuser))
	{
		if (ServerInstance->Channels.IsPrefix(target[0]))
			user->WriteNumeric(Numerics::NoSuchChannel(target));
		else
			user->WriteNumeric(Numerics::NoSuchNick(target));
		return CmdResult::FAILURE;
	}
	if (parameters.size() == 1)
	{
		this->DisplayCurrentModes(user, targetuser, targetchannel);
		return CmdResult::SUCCESS;
	}

	// Populate a temporary Modes::ChangeList with the parameters
	Modes::ChangeList changelist;
	ModeType type = targetchannel ? MODETYPE_CHANNEL : MODETYPE_USER;
	ServerInstance->Modes.ModeParamsToChangeList(user, type, parameters, changelist);

	ModResult modres;
	FIRST_MOD_RESULT(OnPreMode, modres, (user, targetuser, targetchannel, changelist));

	ModeParser::ModeProcessFlag flags = ModeParser::MODE_NONE;
	if (IS_LOCAL(user))
	{
		if (modres == MOD_RES_PASSTHRU)
		{
			if ((targetuser) && (user != targetuser))
			{
				// Local users may only change the modes of other users if a module explicitly allows it
				user->WriteNumeric(ERR_USERSDONTMATCH, "Can't change mode for other users");
				return CmdResult::FAILURE;
			}

			// This is a mode change by a local user and modules didn't explicitly allow/deny.
			// Ensure access checks will happen for each mode being changed.
			flags |= ModeParser::MODE_CHECKACCESS;
		}
		else if (modres == MOD_RES_DENY)
			return CmdResult::FAILURE; // Entire mode change denied by a module
	}
	else
		flags |= ModeParser::MODE_LOCALONLY;

	if (IS_LOCAL(user))
		ServerInstance->Modes.ProcessSingle(user, targetchannel, targetuser, changelist, flags);
	else
		ServerInstance->Modes.Process(user, targetchannel, targetuser, changelist, flags);

	if ((ServerInstance->Modes.GetLastChangeList().empty()) && (targetchannel) && (parameters.size() == 2))
	{
		/* Special case for displaying the list for listmodes,
		 * e.g. MODE #chan b, or MODE #chan +b without a parameter
		 */
		this->DisplayListModes(user, targetchannel, parameters[1]);
	}

	return CmdResult::SUCCESS;
}

RouteDescriptor CommandMode::GetRouting(User* user, const Params& parameters)
{
	return (IS_LOCAL(user) ? ROUTE_LOCALONLY : ROUTE_BROADCAST);
}

void CommandMode::DisplayListModes(User* user, Channel* chan, const std::string& mode_sequence)
{
	seq++;

	for (const auto mletter : mode_sequence)
	{
		if (mletter == '+')
			continue;

		ModeHandler* mh = ServerInstance->Modes.FindMode(mletter, MODETYPE_CHANNEL);
		if (!mh || !mh->IsListMode())
			return;

		/* Ensure the user doesnt request the same mode twice,
		 * so they can't flood themselves off out of idiocy.
		 */
		if (sent[static_cast<unsigned char>(mletter)] == seq)
			continue;

		sent[static_cast<unsigned char>(mletter)] = seq;
		ServerInstance->Modes.ShowListModeList(user, chan, mh);
	}
}

std::string CommandMode::GetSnomasks(const User* user)
{
	std::string snomaskstr = snomaskmode->GetUserParameter(user);
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

		for (const auto& [_, mh] : ServerInstance->Modes.GetModes(MODETYPE_CHANNEL))
		{
			// Check that the mode is set.
			if (!chan->IsModeSet(mh))
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
		user->WriteNumeric(RPL_CHANNELCREATED, targetchannel->name, targetchannel->age);
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

class CoreModMode final
	: public Module
	, public ISupport::EventListener
{
private:
	CommandMode cmdmode;

	static std::string GenerateModeList(ModeType mt)
	{
		// Type A: Modes that add or remove an address to or from a list. These
		// modes MUST always have a parameter when sent from the server to a
		// client. A client MAY issue the mode without an argument to obtain the
		// current contents of the list.
		std::string type1;

		// Type B: Modes that change a setting on a channel. These modes MUST
		// always have a parameter.
		std::string type2;

		// Type C: Modes that change a setting on a channel. These modes MUST
		// have a parameter when being set, and MUST NOT have a parameter when
		// being unset.
		std::string type3;

		// Type D: Modes that change a setting on a channel. These modes MUST
		// NOT have a parameter.
		std::string type4;

		for (const auto& [_, mh] : ServerInstance->Modes.GetModes(mt))
		{
			if (mh->NeedsParam(true))
			{
				PrefixMode* pm = mh->IsPrefixMode();
				if (mh->IsListMode() && (!pm || !pm->GetPrefix()))
				{
					type1 += mh->GetModeChar();
					continue;
				}

				if (mh->NeedsParam(false))
				{
					if (!pm)
						type2 += mh->GetModeChar();
				}
				else
				{
					type3 += mh->GetModeChar();
				}
			}
			else
			{
				type4 += mh->GetModeChar();
			}
		}

		// These don't need to be alphabetically ordered but it looks nicer.
		std::sort(type1.begin(), type1.end());
		std::sort(type2.begin(), type2.end());
		std::sort(type3.begin(), type3.end());
		std::sort(type4.begin(), type4.end());

		return INSP_FORMAT("{},{},{},{}", type1, type2, type3, type4);
	}

	static std::string GeneratePrefixList(bool includemodechars)
	{
		std::vector<PrefixMode*> prefixes;
		for (auto* pm : ServerInstance->Modes.GetPrefixModes())
		{
			if (pm->GetPrefix())
				prefixes.push_back(pm);
		}
		std::sort(prefixes.begin(), prefixes.end(), [](PrefixMode* lhs, PrefixMode* rhs)
		{
			return lhs->GetPrefixRank() < rhs->GetPrefixRank();
		});

		std::string modechars;
		std::string prefixchars;
		for (const auto* pm : insp::reverse_range(prefixes))
		{
			modechars += pm->GetModeChar();
			prefixchars += pm->GetPrefix();
		}

		return includemodechars ? "(" + modechars + ")" + prefixchars : prefixchars;
	}

public:
	CoreModMode()
		: Module(VF_CORE | VF_VENDOR, "Provides the MODE command")
		, ISupport::EventListener(this)
		, cmdmode(this)
	{
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["CHANMODES"] = GenerateModeList(MODETYPE_CHANNEL);
		tokens["USERMODES"] = GenerateModeList(MODETYPE_USER);
		tokens["PREFIX"] = GeneratePrefixList(true);
		tokens["STATUSMSG"] = GeneratePrefixList(false);
	}
};

MODULE_INIT(CoreModMode)
