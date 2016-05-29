/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Shawn Smith <shawn@inspircd.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2004-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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

/* +s (secret) */
/* +p (private) */
/* +m (moderated) */
/* +t (only (half) ops can change topic) */
/* +n (no external messages) */
/* +i (invite only) */
/* +w (see wallops) */
/* +i (invisible) */
#include "modes/simplemodes.h"
/* +b (bans) */
#include "modes/cmode_b.h"
/* +k (keyed channel) */
#include "modes/cmode_k.h"
/* +l (channel user limit) */
#include "modes/cmode_l.h"
/* +o (channel op) */
#include "modes/cmode_o.h"
/* +v (channel voice) */
#include "modes/cmode_v.h"
/* +o (operator) */
#include "modes/umode_o.h"
/* +s (server notice masks) */
#include "modes/umode_s.h"

ModeHandler::ModeHandler(Module* Creator, const std::string& Name, char modeletter, ParamSpec Params, ModeType type)
	: ServiceProvider(Creator, Name, SERVICE_MODE), m_paramtype(TR_TEXT),
	parameters_taken(Params), mode(modeletter), prefix(0), oper(false),
	list(false), m_type(type), levelrequired(HALFOP_VALUE)
{
}

CullResult ModeHandler::cull()
{
	if (ServerInstance->Modes)
		ServerInstance->Modes->DelMode(this);
	return classbase::cull();
}

ModeHandler::~ModeHandler()
{
}

bool ModeHandler::IsListMode()
{
	return list;
}

unsigned int ModeHandler::GetPrefixRank()
{
	return 0;
}

int ModeHandler::GetNumParams(bool adding)
{
	switch (parameters_taken)
	{
		case PARAM_ALWAYS:
			return 1;
		case PARAM_SETONLY:
			return adding ? 1 : 0;
		case PARAM_NONE:
			break;
	}
	return 0;
}

std::string ModeHandler::GetUserParameter(User* user)
{
	return "";
}

ModResult ModeHandler::AccessCheck(User*, Channel*, std::string &, bool)
{
	return MOD_RES_PASSTHRU;
}

ModeAction ModeHandler::OnModeChange(User*, User*, Channel*, std::string&, bool)
{
	return MODEACTION_DENY;
}

void ModeHandler::DisplayList(User*, Channel*)
{
}

void ModeHandler::DisplayEmptyList(User*, Channel*)
{
}

void ModeHandler::OnParameterMissing(User* user, User* dest, Channel* channel)
{
}

bool ModeHandler::ResolveModeConflict(std::string& theirs, const std::string& ours, Channel*)
{
	return (theirs < ours);
}

ModeAction SimpleUserModeHandler::OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
{
	/* We're either trying to add a mode we already have or
		remove a mode we don't have, deny. */
	if (dest->IsModeSet(this->GetModeChar()) == adding)
		return MODEACTION_DENY;

	/* adding will be either true or false, depending on if we
		are adding or removing the mode, since we already checked
		to make sure we aren't adding a mode we have or that we
		aren't removing a mode we don't have, we don't have to do any
		other checks here to see if it's true or false, just add or
		remove the mode */
	dest->SetMode(this->GetModeChar(), adding);

	return MODEACTION_ALLOW;
}


ModeAction SimpleChannelModeHandler::OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
{
	/* We're either trying to add a mode we already have or
		remove a mode we don't have, deny. */
	if (channel->IsModeSet(this->GetModeChar()) == adding)
		return MODEACTION_DENY;

	/* adding will be either true or false, depending on if we
		are adding or removing the mode, since we already checked
		to make sure we aren't adding a mode we have or that we
		aren't removing a mode we don't have, we don't have to do any
		other checks here to see if it's true or false, just add or
		remove the mode */
	channel->SetMode(this->GetModeChar(), adding);

	return MODEACTION_ALLOW;
}

ModeAction ParamChannelModeHandler::OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
{
	if (adding && !ParamValidate(parameter))
		return MODEACTION_DENY;
	std::string now = channel->GetModeParameter(this);
	if (parameter == now)
		return MODEACTION_DENY;
	if (adding)
		channel->SetModeParam(this, parameter);
	else
		channel->SetModeParam(this, "");
	return MODEACTION_ALLOW;
}

bool ParamChannelModeHandler::ParamValidate(std::string& parameter)
{
	return true;
}

ModeWatcher::ModeWatcher(Module* Creator, char modeletter, ModeType type)
	: mode(modeletter), m_type(type), creator(Creator)
{
}

ModeWatcher::~ModeWatcher()
{
}

char ModeWatcher::GetModeChar()
{
	return mode;
}

ModeType ModeWatcher::GetModeType()
{
	return m_type;
}

bool ModeWatcher::BeforeMode(User*, User*, Channel*, std::string&, bool, ModeType)
{
	return true;
}

void ModeWatcher::AfterMode(User*, User*, Channel*, const std::string&, bool, ModeType)
{
}

User* ModeParser::SanityChecks(User *user, const char *dest, Channel *chan, int)
{
	User *d;
	if ((!user) || (!dest) || (!chan) || (!*dest))
	{
		return NULL;
	}
	d = ServerInstance->FindNick(dest);
	if (!d)
	{
		user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",user->nick.c_str(), dest);
		return NULL;
	}
	return d;
}

void ModeParser::DisplayCurrentModes(User *user, User* targetuser, Channel* targetchannel, const char* text)
{
	if (targetchannel)
	{
		/* Display channel's current mode string */
		user->WriteNumeric(RPL_CHANNELMODEIS, "%s %s +%s",user->nick.c_str(), targetchannel->name.c_str(), targetchannel->ChanModes(targetchannel->HasUser(user)));
		user->WriteNumeric(RPL_CHANNELCREATED, "%s %s %lu", user->nick.c_str(), targetchannel->name.c_str(), (unsigned long)targetchannel->age);
		return;
	}
	else
	{
		if (targetuser == user || user->HasPrivPermission("users/auspex"))
		{
			/* Display user's current mode string */
			user->WriteNumeric(RPL_UMODEIS, "%s :+%s",targetuser->nick.c_str(),targetuser->FormatModes());
			if (IS_OPER(targetuser))
				user->WriteNumeric(RPL_SNOMASKIS, "%s +%s :Server notice mask", targetuser->nick.c_str(), targetuser->FormatNoticeMasks());
			return;
		}
		else
		{
			user->WriteNumeric(ERR_USERSDONTMATCH, "%s :Can't view modes for other users", user->nick.c_str());
			return;
		}
	}
}

ModeAction ModeParser::TryMode(User* user, User* targetuser, Channel* chan, bool adding, const unsigned char modechar,
		std::string &parameter, bool SkipACL)
{
	ModeType type = chan ? MODETYPE_CHANNEL : MODETYPE_USER;
	unsigned char mask = chan ? MASK_CHANNEL : MASK_USER;

	ModeHandler *mh = FindMode(modechar, type);
	int pcnt = mh->GetNumParams(adding);

	// crop mode parameter size to 250 characters
	if (parameter.length() > 250 && adding)
		parameter = parameter.substr(0, 250);

	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnRawMode, MOD_RESULT, (user, chan, modechar, parameter, adding, pcnt));

	if (IS_LOCAL(user) && (MOD_RESULT == MOD_RES_DENY))
		return MODEACTION_DENY;

	if (chan && !SkipACL && (MOD_RESULT != MOD_RES_ALLOW))
	{
		MOD_RESULT = mh->AccessCheck(user, chan, parameter, adding);

		if (MOD_RESULT == MOD_RES_DENY)
			return MODEACTION_DENY;
		if (MOD_RESULT == MOD_RES_PASSTHRU)
		{
			unsigned int neededrank = mh->GetLevelRequired();
			/* Compare our rank on the channel against the rank of the required prefix,
			 * allow if >= ours. Because mIRC and xchat throw a tizz if the modes shown
			 * in NAMES(X) are not in rank order, we know the most powerful mode is listed
			 * first, so we don't need to iterate, we just look up the first instead.
			 */
			unsigned int ourrank = chan->GetPrefixValue(user);
			if (ourrank < neededrank)
			{
				ModeHandler* neededmh = NULL;
				for(char c='A'; c <= 'z'; c++)
				{
					ModeHandler *privmh = FindMode(c, MODETYPE_CHANNEL);
					if (privmh && privmh->GetPrefixRank() >= neededrank)
					{
						// this mode is sufficient to allow this action
						if (!neededmh || privmh->GetPrefixRank() < neededmh->GetPrefixRank())
							neededmh = privmh;
					}
				}
				if (neededmh)
					user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must have channel %s access or above to %sset channel mode %c",
						user->nick.c_str(), chan->name.c_str(), neededmh->name.c_str(), adding ? "" : "un", modechar);
				else
					user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You cannot %sset channel mode %c",
						user->nick.c_str(), chan->name.c_str(), adding ? "" : "un", modechar);
				return MODEACTION_DENY;
			}
		}
	}

	unsigned char handler_id = (modechar - 'A') | mask;

	for (ModeWatchIter watchers = modewatchers[handler_id].begin(); watchers != modewatchers[handler_id].end(); watchers++)
	{
		if ((*watchers)->BeforeMode(user, targetuser, chan, parameter, adding, type) == false)
			return MODEACTION_DENY;
		/* A module whacked the parameter completely, and there was one. abort. */
		if (pcnt && parameter.empty())
			return MODEACTION_DENY;
	}

	if (IS_LOCAL(user) && !IS_OPER(user))
	{
		char* disabled = (type == MODETYPE_CHANNEL) ? ServerInstance->Config->DisabledCModes : ServerInstance->Config->DisabledUModes;
		if (disabled[modechar - 'A'])
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - %s mode %c has been locked by the administrator",
				user->nick.c_str(), type == MODETYPE_CHANNEL ? "channel" : "user", modechar);
			return MODEACTION_DENY;
		}
	}

	if (adding && IS_LOCAL(user) && mh->NeedsOper() && !user->HasModePermission(modechar, type))
	{
		/* It's an oper only mode, and they don't have access to it. */
		if (IS_OPER(user))
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - Oper type %s does not have access to set %s mode %c",
					user->nick.c_str(), user->oper->NameStr(), type == MODETYPE_CHANNEL ? "channel" : "user", modechar);
		}
		else
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - Only operators may set %s mode %c",
					user->nick.c_str(), type == MODETYPE_CHANNEL ? "channel" : "user", modechar);
		}
		return MODEACTION_DENY;
	}

	if (mh->GetTranslateType() == TR_NICK)
	{
		User* prefixtarget;
		if (IS_LOCAL(user))
			prefixtarget = ServerInstance->FindNickOnly(parameter);
		else
			prefixtarget = ServerInstance->FindNick(parameter);

		if (!prefixtarget)
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), parameter.c_str());
			return MODEACTION_DENY;
		}
	}

	if (mh->GetPrefixRank() && chan)
	{
		User* user_to_prefix = ServerInstance->FindNick(parameter);
		if (!user_to_prefix)
			return MODEACTION_DENY;
		if (!chan->SetPrefix(user_to_prefix, modechar, adding))
			return MODEACTION_DENY;
	}

	/* Call the handler for the mode */
	ModeAction ma = mh->OnModeChange(user, targetuser, chan, parameter, adding);

	if (pcnt && parameter.empty())
		return MODEACTION_DENY;

	if (ma != MODEACTION_ALLOW)
		return ma;

	for (ModeWatchIter watchers = modewatchers[handler_id].begin(); watchers != modewatchers[handler_id].end(); watchers++)
		(*watchers)->AfterMode(user, targetuser, chan, parameter, adding, type);

	return MODEACTION_ALLOW;
}

void ModeParser::Process(const std::vector<std::string>& parameters, User *user, bool merge)
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
	ModeType type = targetchannel ? MODETYPE_CHANNEL : MODETYPE_USER;

	LastParse.clear();
	LastParseParams.clear();
	LastParseTranslate.clear();

	if ((!targetchannel) && ((!targetuser) || (IS_SERVER(targetuser))))
	{
		user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",user->nick.c_str(),target.c_str());
		return;
	}
	if (parameters.size() == 1)
	{
		this->DisplayCurrentModes(user, targetuser, targetchannel, target.c_str());
		return;
	}

	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnPreMode, MOD_RESULT, (user, targetuser, targetchannel, parameters));

	bool SkipAccessChecks = false;

	if (!IS_LOCAL(user) || ServerInstance->ULine(user->server) || MOD_RESULT == MOD_RES_ALLOW)
		SkipAccessChecks = true;
	else if (MOD_RESULT == MOD_RES_DENY)
		return;

	if (targetuser && !SkipAccessChecks && user != targetuser)
	{
		user->WriteNumeric(ERR_USERSDONTMATCH, "%s :Can't change mode for other users", user->nick.c_str());
		return;
	}

	std::string mode_sequence = parameters[1];

	std::string output_mode;
	std::ostringstream output_parameters;
	LastParseParams.push_back(output_mode);
	LastParseTranslate.push_back(TR_TEXT);

	bool adding = true;
	char output_pm = '\0'; // current output state, '+' or '-'
	unsigned int param_at = 2;

	for (std::string::const_iterator letter = mode_sequence.begin(); letter != mode_sequence.end(); letter++)
	{
		unsigned char modechar = *letter;
		if (modechar == '+' || modechar == '-')
		{
			adding = (modechar == '+');
			continue;
		}

		ModeHandler *mh = this->FindMode(modechar, type);
		if (!mh)
		{
			/* No mode handler? Unknown mode character then. */
			user->WriteServ("%d %s %c :is unknown mode char to me", type == MODETYPE_CHANNEL ? 472 : 501, user->nick.c_str(), modechar);
			continue;
		}

		std::string parameter;
		int pcnt = mh->GetNumParams(adding);
		if (pcnt && param_at == parameters.size())
		{
			/* No parameter, continue to the next mode */
			mh->OnParameterMissing(user, targetuser, targetchannel);
			continue;
		}
		else if (pcnt)
		{
			parameter = parameters[param_at++];
			/* Make sure the user isn't trying to slip in an invalid parameter */
			if ((parameter.empty()) || (parameter.find(':') == 0) || (parameter.rfind(' ') != std::string::npos))
				continue;
			if (merge && targetchannel && targetchannel->IsModeSet(modechar) && !mh->IsListMode())
			{
				std::string ours = targetchannel->GetModeParameter(modechar);
				if (!mh->ResolveModeConflict(parameter, ours, targetchannel))
					/* we won the mode merge, don't apply this mode */
					continue;
			}
		}

		ModeAction ma = TryMode(user, targetuser, targetchannel, adding, modechar, parameter, SkipAccessChecks);

		if (ma != MODEACTION_ALLOW)
			continue;

		char needed_pm = adding ? '+' : '-';
		if (needed_pm != output_pm)
		{
			output_pm = needed_pm;
			output_mode.append(1, output_pm);
		}
		output_mode.append(1, modechar);

		if (pcnt)
		{
			TranslateType tt = mh->GetTranslateType();
			if (tt == TR_NICK)
			{
				User* u = ServerInstance->FindNick(parameter);
				if (u)
					parameter = u->nick;
			}
			output_parameters << " " << parameter;
			LastParseParams.push_back(parameter);
			LastParseTranslate.push_back(tt);
		}

		if ( (output_mode.length() + output_parameters.str().length() > 450)
				|| (output_mode.length() > 100)
				|| (LastParseParams.size() > ServerInstance->Config->Limits.MaxModes))
		{
			/* mode sequence is getting too long */
			break;
		}
	}

	LastParseParams[0] = output_mode;

	if (!output_mode.empty())
	{
		LastParse = targetchannel ? targetchannel->name : targetuser->nick;
		LastParse.append(" ");
		LastParse.append(output_mode);
		LastParse.append(output_parameters.str());

		if (targetchannel)
		{
			targetchannel->WriteChannel(user, "MODE %s", LastParse.c_str());
			FOREACH_MOD(I_OnMode,OnMode(user, targetchannel, TYPE_CHANNEL, LastParseParams, LastParseTranslate));
		}
		else
		{
			targetuser->WriteFrom(user, "MODE %s", LastParse.c_str());
			FOREACH_MOD(I_OnMode,OnMode(user, targetuser, TYPE_USER, LastParseParams, LastParseTranslate));
		}
	}
	else if (targetchannel && parameters.size() == 2)
	{
		/* Special case for displaying the list for listmodes,
		 * e.g. MODE #chan b, or MODE #chan +b without a parameter
		 */
		this->DisplayListModes(user, targetchannel, mode_sequence);
	}
}

void ModeParser::DisplayListModes(User* user, Channel* chan, std::string &mode_sequence)
{
	seq++;

	for (std::string::const_iterator letter = mode_sequence.begin(); letter != mode_sequence.end(); letter++)
	{
		unsigned char mletter = *letter;
		if (mletter == '+')
			continue;

		/* Ensure the user doesnt request the same mode twice,
		 * so they cant flood themselves off out of idiocy.
		 */
		if (sent[mletter] == seq)
			continue;

		sent[mletter] = seq;

		ModeHandler *mh = this->FindMode(mletter, MODETYPE_CHANNEL);

		if (!mh || !mh->IsListMode())
			return;

		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnRawMode, MOD_RESULT, (user, chan, mletter, "", true, 0));
		if (MOD_RESULT == MOD_RES_DENY)
			continue;

		bool display = true;
		if (!user->HasPrivPermission("channels/auspex") && ServerInstance->Config->HideModeLists[mletter] && (chan->GetPrefixValue(user) < HALFOP_VALUE))
		{
			user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You do not have access to view the +%c list",
				user->nick.c_str(), chan->name.c_str(), mletter);
			display = false;
		}

		unsigned char handler_id = (mletter - 'A') | MASK_CHANNEL;

		for(ModeWatchIter watchers = modewatchers[handler_id].begin(); watchers != modewatchers[handler_id].end(); watchers++)
		{
			std::string dummyparam;

			if (!((*watchers)->BeforeMode(user, NULL, chan, dummyparam, true, MODETYPE_CHANNEL)))
				display = false;
		}
		if (display)
			mh->DisplayList(user, chan);
		else
			mh->DisplayEmptyList(user, chan);
	}
}

const std::string& ModeParser::GetLastParse()
{
	return LastParse;
}

void ModeParser::CleanMask(std::string &mask)
{
	std::string::size_type pos_of_pling = mask.find_first_of('!');
	std::string::size_type pos_of_at = mask.find_first_of('@');
	std::string::size_type pos_of_dot = mask.find_first_of('.');
	std::string::size_type pos_of_colons = mask.find("::"); /* Because ipv6 addresses are colon delimited -- double so it treats extban as nick */

	if (mask.length() >= 2 && mask[1] == ':')
		return; // if it's an extban, don't even try guess how it needs to be formed.

	if ((pos_of_pling == std::string::npos) && (pos_of_at == std::string::npos))
	{
		/* Just a nick, or just a host - or clearly ipv6 (starting with :) */
		if ((pos_of_dot == std::string::npos) && (pos_of_colons == std::string::npos) && mask[0] != ':')
		{
			/* It has no '.' in it, it must be a nick. */
			mask.append("!*@*");
		}
		else
		{
			/* Got a dot in it? Has to be a host */
			mask = "*!*@" + mask;
		}
	}
	else if ((pos_of_pling == std::string::npos) && (pos_of_at != std::string::npos))
	{
		/* Has an @ but no !, its a user@host */
		 mask = "*!" + mask;
	}
	else if ((pos_of_pling != std::string::npos) && (pos_of_at == std::string::npos))
	{
		/* Has a ! but no @, it must be a nick!ident */
		mask.append("@*");
	}
}

bool ModeParser::AddMode(ModeHandler* mh)
{
	unsigned char mask = 0;
	unsigned char pos = 0;

	/* Yes, i know, this might let people declare modes like '_' or '^'.
	 * If they do that, thats their problem, and if i ever EVER see an
	 * official InspIRCd developer do that, i'll beat them with a paddle!
	 */
	if ((mh->GetModeChar() < 'A') || (mh->GetModeChar() > 'z') || (mh->GetPrefix() > 126))
		return false;

	/* A mode prefix of ',' is not acceptable, it would fuck up server to server.
	 * A mode prefix of ':' will fuck up both server to server, and client to server.
	 * A mode prefix of '#' will mess up /whois and /privmsg
	 */
	if ((mh->GetPrefix() == ',') || (mh->GetPrefix() == ':') || (mh->GetPrefix() == '#'))
		return false;

	if (mh->GetPrefix() && FindPrefix(mh->GetPrefix()))
		return false;

	mh->GetModeType() == MODETYPE_USER ? mask = MASK_USER : mask = MASK_CHANNEL;
	pos = (mh->GetModeChar()-65) | mask;

	if (modehandlers[pos])
		return false;

	modehandlers[pos] = mh;
	return true;
}

bool ModeParser::DelMode(ModeHandler* mh)
{
	unsigned char mask = 0;
	unsigned char pos = 0;

	if ((mh->GetModeChar() < 'A') || (mh->GetModeChar() > 'z'))
		return false;

	mh->GetModeType() == MODETYPE_USER ? mask = MASK_USER : mask = MASK_CHANNEL;
	pos = (mh->GetModeChar()-65) | mask;

	if (modehandlers[pos] != mh)
		return false;

	/* Note: We can't stack here, as we have modes potentially being removed across many different channels.
	 * To stack here we have to make the algorithm slower. Discuss.
	 */
	switch (mh->GetModeType())
	{
		case MODETYPE_USER:
			for (user_hash::iterator i = ServerInstance->Users->clientlist->begin(); i != ServerInstance->Users->clientlist->end(); )
			{
				User* user = i->second;
				++i;
				mh->RemoveMode(user);
			}
		break;
		case MODETYPE_CHANNEL:
			for (chan_hash::iterator i = ServerInstance->chanlist->begin(); i != ServerInstance->chanlist->end(); )
			{
				// The channel may not be in the hash after RemoveMode(), see m_permchannels
				Channel* chan = i->second;
				++i;
				mh->RemoveMode(chan);
			}
		break;
	}

	modehandlers[pos] = NULL;

	return true;
}

ModeHandler* ModeParser::FindMode(unsigned const char modeletter, ModeType mt)
{
	unsigned char mask = 0;
	unsigned char pos = 0;

	if ((modeletter < 'A') || (modeletter > 'z'))
		return NULL;

	mt == MODETYPE_USER ? mask = MASK_USER : mask = MASK_CHANNEL;
	pos = (modeletter-65) | mask;

	return modehandlers[pos];
}

std::string ModeParser::UserModeList()
{
	char modestr[256];
	int pointer = 0;

	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		unsigned char pos = (mode-65) | MASK_USER;

		if (modehandlers[pos])
			modestr[pointer++] = mode;
	}
	modestr[pointer++] = 0;
	return modestr;
}

std::string ModeParser::ChannelModeList()
{
	char modestr[256];
	int pointer = 0;

	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		unsigned char pos = (mode-65) | MASK_CHANNEL;

		if (modehandlers[pos])
			modestr[pointer++] = mode;
	}
	modestr[pointer++] = 0;
	return modestr;
}

std::string ModeParser::ParaModeList()
{
	char modestr[256];
	int pointer = 0;

	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		unsigned char pos = (mode-65) | MASK_CHANNEL;

		if ((modehandlers[pos]) && (modehandlers[pos]->GetNumParams(true)))
			modestr[pointer++] = mode;
	}
	modestr[pointer++] = 0;
	return modestr;
}

ModeHandler* ModeParser::FindPrefix(unsigned const char pfxletter)
{
	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		unsigned char pos = (mode-65) | MASK_CHANNEL;

		if ((modehandlers[pos]) && (modehandlers[pos]->GetPrefix() == pfxletter))
		{
			return modehandlers[pos];
		}
	}
	return NULL;
}

std::string ModeParser::GiveModeList(ModeMasks m)
{
	std::string type1;	/* Listmodes EXCEPT those with a prefix */
	std::string type2;	/* Modes that take a param when adding or removing */
	std::string type3;	/* Modes that only take a param when adding */
	std::string type4;	/* Modes that dont take a param */

	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		unsigned char pos = (mode-65) | m;
		 /* One parameter when adding */
		if (modehandlers[pos])
		{
			if (modehandlers[pos]->GetNumParams(true))
			{
				if ((modehandlers[pos]->IsListMode()) && (!modehandlers[pos]->GetPrefix()))
				{
					type1 += modehandlers[pos]->GetModeChar();
				}
				else
				{
					/* ... and one parameter when removing */
					if (modehandlers[pos]->GetNumParams(false))
					{
						/* But not a list mode */
						if (!modehandlers[pos]->GetPrefix())
						{
							type2 += modehandlers[pos]->GetModeChar();
						}
					}
					else
					{
						/* No parameters when removing */
						type3 += modehandlers[pos]->GetModeChar();
					}
				}
			}
			else
			{
				type4 += modehandlers[pos]->GetModeChar();
			}
		}
	}

	return type1 + "," + type2 + "," + type3 + "," + type4;
}

struct PrefixModeSorter
{
	bool operator()(ModeHandler* lhs, ModeHandler* rhs)
	{
		return lhs->GetPrefixRank() < rhs->GetPrefixRank();
	}
};

std::string ModeParser::BuildPrefixes(bool lettersAndModes)
{
	std::string mletters;
	std::string mprefixes;
	std::vector<ModeHandler*> prefixes;

	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		unsigned char pos = (mode-65) | MASK_CHANNEL;

		if ((modehandlers[pos]) && (modehandlers[pos]->GetPrefix()))
		{
			prefixes.push_back(modehandlers[pos]);
		}
	}

	std::sort(prefixes.begin(), prefixes.end(), PrefixModeSorter());
	for (std::vector<ModeHandler*>::const_reverse_iterator n = prefixes.rbegin(); n != prefixes.rend(); ++n)
	{
		mletters += (*n)->GetPrefix();
		mprefixes += (*n)->GetModeChar();
	}

	return lettersAndModes ? "(" + mprefixes + ")" + mletters : mletters;
}

bool ModeParser::AddModeWatcher(ModeWatcher* mw)
{
	unsigned char mask = 0;
	unsigned char pos = 0;

	if (!mw)
		return false;

	if ((mw->GetModeChar() < 'A') || (mw->GetModeChar() > 'z'))
		return false;

	mw->GetModeType() == MODETYPE_USER ? mask = MASK_USER : mask = MASK_CHANNEL;
	pos = (mw->GetModeChar()-65) | mask;

	modewatchers[pos].push_back(mw);

	return true;
}

bool ModeParser::DelModeWatcher(ModeWatcher* mw)
{
	unsigned char mask = 0;
	unsigned char pos = 0;

	if (!mw)
		return false;

	if ((mw->GetModeChar() < 'A') || (mw->GetModeChar() > 'z'))
		return false;

	mw->GetModeType() == MODETYPE_USER ? mask = MASK_USER : mask = MASK_CHANNEL;
	pos = (mw->GetModeChar()-65) | mask;

	ModeWatchIter a = std::find(modewatchers[pos].begin(),modewatchers[pos].end(),mw);

	if (a == modewatchers[pos].end())
	{
		return false;
	}

	modewatchers[pos].erase(a);

	return true;
}

/** This default implementation can remove simple user modes
 */
void ModeHandler::RemoveMode(User* user, irc::modestacker* stack)
{
	if (user->IsModeSet(this->GetModeChar()))
	{
		if (stack)
		{
			stack->Push(this->GetModeChar());
		}
		else
		{
			std::vector<std::string> parameters;
			parameters.push_back(user->nick);
			parameters.push_back("-");
			parameters[1].push_back(this->GetModeChar());
			ServerInstance->Modes->Process(parameters, ServerInstance->FakeClient);
		}
	}
}

/** This default implementation can remove simple channel modes
 * (no parameters)
 */
void ModeHandler::RemoveMode(Channel* channel, irc::modestacker* stack)
{
	if (channel->IsModeSet(this->GetModeChar()))
	{
		if (stack)
		{
			stack->Push(this->GetModeChar());
		}
		else
		{
			std::vector<std::string> parameters;
			parameters.push_back(channel->name);
			parameters.push_back("-");
			parameters[1].push_back(this->GetModeChar());
			ServerInstance->SendMode(parameters, ServerInstance->FakeClient);
		}
	}
}

struct builtin_modes
{
	ModeChannelSecret s;
	ModeChannelPrivate p;
	ModeChannelModerated m;
	ModeChannelTopicOps t;

	ModeChannelNoExternal n;
	ModeChannelInviteOnly i;
	ModeChannelKey k;
	ModeChannelLimit l;

	ModeChannelBan b;
	ModeChannelOp o;
	ModeChannelVoice v;

	ModeUserWallops uw;
	ModeUserInvisible ui;
	ModeUserOperator uo;
	ModeUserServerNoticeMask us;

	void init(ModeParser* modes)
	{
		modes->AddMode(&s);
		modes->AddMode(&p);
		modes->AddMode(&m);
		modes->AddMode(&t);
		modes->AddMode(&n);
		modes->AddMode(&i);
		modes->AddMode(&k);
		modes->AddMode(&l);
		modes->AddMode(&b);
		modes->AddMode(&o);
		modes->AddMode(&v);
		modes->AddMode(&uw);
		modes->AddMode(&ui);
		modes->AddMode(&uo);
		modes->AddMode(&us);
	}
};

static builtin_modes static_modes;

ModeParser::ModeParser()
{
	/* Clear mode handler list */
	memset(modehandlers, 0, sizeof(modehandlers));

	/* Last parse string */
	LastParse.clear();

	seq = 0;
	memset(&sent, 0, sizeof(sent));

	static_modes.init(this);
}

ModeParser::~ModeParser()
{
}
