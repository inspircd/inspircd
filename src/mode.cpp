/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2016-2019, 2021-2022, 2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Shawn Smith <ShawnSmith0828@gmail.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
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

ModeHandler::ModeHandler(Module* Creator, const std::string& Name, char modeletter, ParamSpec Params, ModeType type, Class mclass)
	: ServiceProvider(Creator, Name, SERVICE_MODE)
	, modeid(ModeParser::MODEID_MAX)
	, parameters_taken(Params)
	, mode(modeletter)
	, oper(false)
	, list(false)
	, m_type(type)
	, type_id(mclass)
	, ranktoset(HALFOP_VALUE)
	, ranktounset(HALFOP_VALUE)
{
}

CullResult ModeHandler::cull()
{
	if (ServerInstance)
		ServerInstance->Modes->DelMode(this);
	return classbase::cull();
}

ModeHandler::~ModeHandler()
{
}

bool ModeHandler::NeedsParam(bool adding) const
{
	switch (parameters_taken)
	{
		case PARAM_ALWAYS:
			return true;
		case PARAM_SETONLY:
			return adding;
		case PARAM_NONE:
			break;
	}
	return false;
}

std::string ModeHandler::GetUserParameter(const User* user) const
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
	std::string message = InspIRCd::Format("You must specify a parameter for the %s mode.", name.c_str());
	if (!syntax.empty())
		message.append(InspIRCd::Format(" Syntax: %s.", syntax.c_str()));

	if (channel)
		user->WriteNumeric(Numerics::InvalidModeParameter(channel, this, "*", message));
	else
		user->WriteNumeric(Numerics::InvalidModeParameter(dest, this, "*", message));
}

void ModeHandler::OnParameterInvalid(User* user, Channel* targetchannel, User* targetuser, const std::string& parameter)
{
	if (targetchannel)
		user->WriteNumeric(Numerics::InvalidModeParameter(targetchannel, this, "*"));
	else
		user->WriteNumeric(Numerics::InvalidModeParameter(targetuser, this, "*"));
}

bool ModeHandler::ResolveModeConflict(std::string& theirs, const std::string& ours, Channel*)
{
	return (theirs < ours);
}

void ModeHandler::RegisterService()
{
	ServerInstance->Modes.AddMode(this);
	ServerInstance->Modules.AddReferent((GetModeType() == MODETYPE_CHANNEL ? "mode/" : "umode/") + name, this);
}

ModeAction SimpleUserModeHandler::OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
{
	/* We're either trying to add a mode we already have or
		remove a mode we don't have, deny. */
	if (dest->IsModeSet(this) == adding)
		return MODEACTION_DENY;

	/* adding will be either true or false, depending on if we
		are adding or removing the mode, since we already checked
		to make sure we aren't adding a mode we have or that we
		aren't removing a mode we don't have, we don't have to do any
		other checks here to see if it's true or false, just add or
		remove the mode */
	dest->SetMode(this, adding);

	return MODEACTION_ALLOW;
}


ModeAction SimpleChannelModeHandler::OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
{
	/* We're either trying to add a mode we already have or
		remove a mode we don't have, deny. */
	if (channel->IsModeSet(this) == adding)
		return MODEACTION_DENY;

	/* adding will be either true or false, depending on if we
		are adding or removing the mode, since we already checked
		to make sure we aren't adding a mode we have or that we
		aren't removing a mode we don't have, we don't have to do any
		other checks here to see if it's true or false, just add or
		remove the mode */
	channel->SetMode(this, adding);

	return MODEACTION_ALLOW;
}

ModeWatcher::ModeWatcher(Module* Creator, const std::string& modename, ModeType type)
	: mode(modename), m_type(type), creator(Creator)
{
	ServerInstance->Modes->AddModeWatcher(this);
}

ModeWatcher::~ModeWatcher()
{
	ServerInstance->Modes->DelModeWatcher(this);
}

bool ModeWatcher::BeforeMode(User*, User*, Channel*, std::string&, bool)
{
	return true;
}

void ModeWatcher::AfterMode(User*, User*, Channel*, const std::string&, bool)
{
}

PrefixMode::PrefixMode(Module* Creator, const std::string& Name, char ModeLetter, unsigned int Rank, char PrefixChar)
	: ModeHandler(Creator, Name, ModeLetter, PARAM_ALWAYS, MODETYPE_CHANNEL, MC_PREFIX)
	, prefix(PrefixChar)
	, prefixrank(Rank)
	, selfremove(true)
{
	list = true;
	syntax = "<nick>";
}

ModResult PrefixMode::AccessCheck(User* src, Channel*, std::string& value, bool adding)
{
	if (!adding && src->nick == value && selfremove)
		return MOD_RES_ALLOW;
	return MOD_RES_PASSTHRU;
}

ModeAction PrefixMode::OnModeChange(User* source, User*, Channel* chan, std::string& parameter, bool adding)
{
	User* target;
	if (IS_LOCAL(source))
		target = ServerInstance->FindNickOnly(parameter);
	else
		target = ServerInstance->FindNick(parameter);

	if (!target)
	{
		source->WriteNumeric(Numerics::NoSuchNick(parameter));
		return MODEACTION_DENY;
	}

	Membership* memb = chan->GetUser(target);
	if (!memb)
	{
		source->WriteNumeric(ERR_USERNOTINCHANNEL, target->nick, chan->name, "They are not on that channel");
		return MODEACTION_DENY;
	}

	parameter = target->nick;
	return (memb->SetPrefix(this, adding) ? MODEACTION_ALLOW : MODEACTION_DENY);
}

void PrefixMode::Update(unsigned int rank, unsigned int setrank, unsigned int unsetrank, bool selfrm)
{
	prefixrank = rank;
	ranktoset = setrank;
	ranktounset = unsetrank;
	selfremove = selfrm;
}

ModeAction ParamModeBase::OnModeChange(User* source, User*, Channel* chan, std::string& parameter, bool adding)
{
	if (adding)
	{
		if (chan->GetModeParameter(this) == parameter)
			return MODEACTION_DENY;

		if (OnSet(source, chan, parameter) != MODEACTION_ALLOW)
			return MODEACTION_DENY;

		chan->SetMode(this, true);

		// Handler might have changed the parameter internally
		parameter.clear();
		this->GetParameter(chan, parameter);
	}
	else
	{
		if (!chan->IsModeSet(this))
			return MODEACTION_DENY;
		this->OnUnsetInternal(source, chan);
		chan->SetMode(this, false);
	}
	return MODEACTION_ALLOW;
}

ModeAction ModeParser::TryMode(User* user, User* targetuser, Channel* chan, Modes::Change& mcitem, bool SkipACL)
{
	ModeType type = chan ? MODETYPE_CHANNEL : MODETYPE_USER;

	ModeHandler* mh = mcitem.mh;
	bool adding = mcitem.adding;
	const bool needs_param = mh->NeedsParam(adding);

	std::string& parameter = mcitem.param;
	// crop mode parameter size to MODE_PARAM_MAX characters
	if (parameter.length() > MODE_PARAM_MAX && adding)
		parameter.erase(MODE_PARAM_MAX);

	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnRawMode, MOD_RESULT, (user, chan, mh, parameter, adding));

	if (IS_LOCAL(user) && (MOD_RESULT == MOD_RES_DENY))
		return MODEACTION_DENY;

	const char modechar = mh->GetModeChar();

	if (chan && !SkipACL && (MOD_RESULT != MOD_RES_ALLOW))
	{
		MOD_RESULT = mh->AccessCheck(user, chan, parameter, adding);

		if (MOD_RESULT == MOD_RES_DENY)
			return MODEACTION_DENY;
		if (MOD_RESULT == MOD_RES_PASSTHRU)
		{
			unsigned int neededrank = mh->GetLevelRequired(adding);
			unsigned int ourrank = chan->GetPrefixValue(user);
			if (ourrank < neededrank)
			{
				user->WriteNumeric(Numerics::ChannelPrivilegesNeeded(chan, neededrank, InspIRCd::Format("%s channel mode %c (%s)",
					adding ? "set" : "unset", mh->GetModeChar(), mh->name.c_str())));
				return MODEACTION_DENY;
			}
		}
	}

	// Ask mode watchers whether this mode change is OK
	std::pair<ModeWatcherMap::iterator, ModeWatcherMap::iterator> itpair = modewatchermap.equal_range(mh->name);
	for (ModeWatcherMap::iterator i = itpair.first; i != itpair.second; ++i)
	{
		ModeWatcher* mw = i->second;
		if (mw->GetModeType() == type)
		{
			if (!mw->BeforeMode(user, targetuser, chan, parameter, adding))
				return MODEACTION_DENY;

			// A module whacked the parameter completely, and there was one. Abort.
			if ((needs_param) && (parameter.empty()))
				return MODEACTION_DENY;
		}
	}

	if ((chan || (!chan && adding)) && IS_LOCAL(user) && mh->NeedsOper() && !user->HasModePermission(mh))
	{
		/* It's an oper only mode, and they don't have access to it. */
		if (user->IsOper())
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, InspIRCd::Format("Permission Denied - Oper type %s does not have access to %sset %s mode %c",
					user->oper->name.c_str(), adding ? "" : "un", type == MODETYPE_CHANNEL ? "channel" : "user", modechar));
		}
		else
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, InspIRCd::Format("Permission Denied - Only operators may %sset %s mode %c",
					adding ? "" : "un", type == MODETYPE_CHANNEL ? "channel" : "user", modechar));
		}
		return MODEACTION_DENY;
	}

	/* Call the handler for the mode */
	ModeAction ma = mh->OnModeChange(user, targetuser, chan, parameter, adding);

	if ((needs_param) && (parameter.empty()))
		return MODEACTION_DENY;

	if (ma != MODEACTION_ALLOW)
		return ma;

	itpair = modewatchermap.equal_range(mh->name);
	for (ModeWatcherMap::iterator i = itpair.first; i != itpair.second; ++i)
	{
		ModeWatcher* mw = i->second;
		if (mw->GetModeType() == type)
			mw->AfterMode(user, targetuser, chan, parameter, adding);
	}

	return MODEACTION_ALLOW;
}

void ModeParser::ModeParamsToChangeList(User* user, ModeType type, const std::vector<std::string>& parameters, Modes::ChangeList& changelist, unsigned int beginindex, unsigned int endindex)
{
	if (endindex > parameters.size())
		endindex = parameters.size();

	const std::string& mode_sequence = parameters[beginindex];

	bool adding = true;
	unsigned int param_at = beginindex+1;

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
			int numeric = (type == MODETYPE_CHANNEL ? ERR_UNKNOWNMODE : ERR_UNKNOWNSNOMASK);
			const char* typestr = (type == MODETYPE_CHANNEL ? "channel" : "user");
			user->WriteNumeric(numeric, modechar, InspIRCd::Format("is not a recognised %s mode.", typestr));
			continue;
		}

		std::string parameter;
		if ((mh->NeedsParam(adding)) && (param_at < endindex))
			parameter = parameters[param_at++];

		changelist.push(mh, adding, parameter);
	}
}

static bool IsModeParamValid(User* user, Channel* targetchannel, User* targetuser, const Modes::Change& item)
{
	// An empty parameter is never acceptable
	if (item.param.empty())
	{
		item.mh->OnParameterMissing(user, targetuser, targetchannel);
		return false;
	}

	// The parameter cannot begin with a ':' character or contain a space
	if ((item.param[0] == ':') || (item.param.find(' ') != std::string::npos))
	{
		item.mh->OnParameterInvalid(user, targetchannel, targetuser, item.param);
		return false;
	}

	return true;
}

// Returns true if we should apply a merged mode, false if we should skip it
static bool ShouldApplyMergedMode(Channel* chan, Modes::Change& item)
{
	ModeHandler* mh = item.mh;
	if ((!chan) || (!chan->IsModeSet(mh)) || (mh->IsListMode()))
		// Mode not set here or merge is not applicable, apply the incoming mode
		return true;

	// Mode handler decides
	std::string ours = chan->GetModeParameter(mh);
	return mh->ResolveModeConflict(item.param, ours, chan);
}

void ModeParser::Process(User* user, Channel* targetchannel, User* targetuser, Modes::ChangeList& changelist, ModeProcessFlag flags)
{
	// Call ProcessSingle until the entire list is processed, but at least once to ensure
	// LastParse and LastChangeList are cleared
	unsigned int processed = 0;
	do
	{
		unsigned int n = ProcessSingle(user, targetchannel, targetuser, changelist, flags, processed);
		processed += n;
	}
	while (processed < changelist.size());
}

unsigned int ModeParser::ProcessSingle(User* user, Channel* targetchannel, User* targetuser, Modes::ChangeList& changelist, ModeProcessFlag flags, unsigned int beginindex)
{
	LastChangeList.clear();

	unsigned int modes_processed = 0;
	Modes::ChangeList::List& list = changelist.getlist();
	for (Modes::ChangeList::List::iterator i = list.begin()+beginindex; i != list.end(); ++i)
	{
		modes_processed++;

		Modes::Change& item = *i;
		ModeHandler* mh = item.mh;

		// If a mode change has been given for a mode that does not exist then reject
		// it. This can happen when core_reloadmodule attempts to restore a mode that
		// no longer exists.
		if (!mh)
			continue;

		// If the mode is supposed to have a parameter then we first take a look at item.param
		// and, if we were asked to, also handle mode merges now
		if (mh->NeedsParam(item.adding))
		{
			// Skip the mode if the parameter does not pass basic validation
			if (!IsModeParamValid(user, targetchannel, targetuser, item))
				continue;

			// If this is a merge and we won we don't apply this mode
			if ((flags & MODE_MERGE) && (!ShouldApplyMergedMode(targetchannel, item)))
				continue;
		}

		ModeAction ma = TryMode(user, targetuser, targetchannel, item, (!(flags & MODE_CHECKACCESS)));

		if (ma != MODEACTION_ALLOW)
			continue;

		LastChangeList.push(mh, item.adding, item.param);

		if (LastChangeList.size() >= ServerInstance->Config->Limits.MaxModes)
		{
			/* mode sequence is getting too long */
			break;
		}
	}

	if (!LastChangeList.empty())
	{
		ClientProtocol::Events::Mode modeevent(user, targetchannel, targetuser, LastChangeList);
		if (targetchannel)
		{
			targetchannel->Write(modeevent);
		}
		else
		{
			LocalUser* localtarget = IS_LOCAL(targetuser);
			if (localtarget)
				localtarget->Send(modeevent);
		}

		FOREACH_MOD(OnMode, (user, targetuser, targetchannel, LastChangeList, flags));
	}

	return modes_processed;
}

void ModeParser::ShowListModeList(User* user, Channel* chan, ModeHandler* mh)
{
	{
		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnRawMode, MOD_RESULT, (user, chan, mh, "", true));
		if (MOD_RESULT == MOD_RES_DENY)
			return;

		bool display = true;

		// Ask mode watchers whether it's OK to show the list
		std::pair<ModeWatcherMap::iterator, ModeWatcherMap::iterator> itpair = modewatchermap.equal_range(mh->name);
		for (ModeWatcherMap::iterator i = itpair.first; i != itpair.second; ++i)
		{
			ModeWatcher* mw = i->second;
			if (mw->GetModeType() == MODETYPE_CHANNEL)
			{
				std::string dummyparam;

				if (!mw->BeforeMode(user, NULL, chan, dummyparam, true))
				{
					// A mode watcher doesn't want us to show the list
					display = false;
					break;
				}
			}
		}

		if (display)
			mh->DisplayList(user, chan);
		else
			mh->DisplayEmptyList(user, chan);
	}
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

ModeHandler::Id ModeParser::AllocateModeId(ModeType mt)
{
	for (ModeHandler::Id i = 0; i != MODEID_MAX; ++i)
	{
		if (!modehandlersbyid[mt][i])
			return i;
	}

	throw ModuleException("Out of ModeIds");
}

void ModeParser::AddMode(ModeHandler* mh)
{
	if (!ModeParser::IsModeChar(mh->GetModeChar()))
		throw ModuleException(InspIRCd::Format("Mode letter for %s is invalid: %c",
			mh->name.c_str(), mh->GetModeChar()));

	/* A mode prefix of ',' is not acceptable, it would fuck up server to server.
	 * A mode prefix of ':' will fuck up both server to server, and client to server.
	 * A mode prefix of '#' will mess up /whois and /privmsg
	 */
	PrefixMode* pm = mh->IsPrefixMode();
	if (pm)
	{
		if ((pm->GetPrefix() > 126) || (pm->GetPrefix() == ',') || (pm->GetPrefix() == ':') || (pm->GetPrefix() == '#'))
			throw ModuleException(InspIRCd::Format("Mode prefix for %s is invalid: %c",
				mh->name.c_str(), pm->GetPrefix()));

		PrefixMode* otherpm = FindPrefix(pm->GetPrefix());
		if (otherpm)
			throw ModuleException(InspIRCd::Format("Mode prefix for %s already used by %s from %s: %c",
				mh->name.c_str(), otherpm->name.c_str(), otherpm->creator->ModuleSourceFile.c_str(), pm->GetPrefix()));
	}

	ModeHandler*& slot = modehandlers[mh->GetModeType()][mh->GetModeChar()-65];
	if (slot)
		throw ModuleException(InspIRCd::Format("Mode letter for %s already used by %s from %s: %c",
			mh->name.c_str(), slot->name.c_str(), slot->creator->ModuleSourceFile.c_str(), mh->GetModeChar()));

	// The mode needs an id if it is either a user mode, a simple mode (flag) or a parameter mode.
	// Otherwise (for listmodes and prefix modes) the id remains MODEID_MAX, which is invalid.
	ModeHandler::Id modeid = MODEID_MAX;
	if ((mh->GetModeType() == MODETYPE_USER) || (mh->IsParameterMode()) || (!mh->IsListMode()))
		modeid = AllocateModeId(mh->GetModeType());

	std::pair<ModeHandlerMap::iterator, bool> res = modehandlersbyname[mh->GetModeType()].insert(std::make_pair(mh->name, mh));
	if (!res.second)
	{
		ModeHandler* othermh = res.first->second;
		throw ModuleException(InspIRCd::Format("Mode name %s already used by %c from %s",
			mh->name.c_str(), othermh->GetModeChar(), othermh->creator->ModuleSourceFile.c_str()));
	}

	// Everything is fine, add the mode

	// If we allocated an id for this mode then save it and put the mode handler into the slot
	if (modeid != MODEID_MAX)
	{
		mh->modeid = modeid;
		modehandlersbyid[mh->GetModeType()][modeid] = mh;
	}

	slot = mh;
	if (pm)
		mhlist.prefix.push_back(pm);
	else if (mh->IsListModeBase())
		mhlist.list.push_back(mh->IsListModeBase());
}

bool ModeParser::DelMode(ModeHandler* mh)
{
	if (!ModeParser::IsModeChar(mh->GetModeChar()))
		return false;

	ModeHandlerMap& mhmap = modehandlersbyname[mh->GetModeType()];
	ModeHandlerMap::iterator mhmapit = mhmap.find(mh->name);
	if ((mhmapit == mhmap.end()) || (mhmapit->second != mh))
		return false;

	ModeHandler*& slot = modehandlers[mh->GetModeType()][mh->GetModeChar()-65];
	if (slot != mh)
		return false;

	/* Note: We can't stack here, as we have modes potentially being removed across many different channels.
	 * To stack here we have to make the algorithm slower. Discuss.
	 */
	switch (mh->GetModeType())
	{
		case MODETYPE_USER:
		{
			const user_hash& users = ServerInstance->Users->GetUsers();
			for (user_hash::const_iterator i = users.begin(); i != users.end(); )
			{
				User* user = i->second;
				++i;
				mh->RemoveMode(user);
			}
		}
		break;
		case MODETYPE_CHANNEL:
		{
			const chan_hash& chans = ServerInstance->GetChans();
			for (chan_hash::const_iterator i = chans.begin(); i != chans.end(); )
			{
				// The channel may not be in the hash after RemoveMode(), see m_permchannels
				Channel* chan = i->second;
				++i;

				Modes::ChangeList changelist;
				mh->RemoveMode(chan, changelist);
				this->Process(ServerInstance->FakeClient, chan, NULL, changelist, MODE_LOCALONLY);
			}
		}
		break;
	}

	mhmap.erase(mhmapit);
	if (mh->GetId() != MODEID_MAX)
		modehandlersbyid[mh->GetModeType()][mh->GetId()] = NULL;
	slot = NULL;
	if (mh->IsPrefixMode())
		mhlist.prefix.erase(std::find(mhlist.prefix.begin(), mhlist.prefix.end(), mh->IsPrefixMode()));
	else if (mh->IsListModeBase())
		mhlist.list.erase(std::find(mhlist.list.begin(), mhlist.list.end(), mh->IsListModeBase()));
	return true;
}

ModeHandler* ModeParser::FindMode(const std::string& modename, ModeType mt)
{
	ModeHandlerMap& mhmap = modehandlersbyname[mt];
	ModeHandlerMap::const_iterator it = mhmap.find(modename);
	if (it != mhmap.end())
		return it->second;

	return NULL;
}

ModeHandler* ModeParser::FindMode(unsigned const char modeletter, ModeType mt)
{
	if (!ModeParser::IsModeChar(modeletter))
		return NULL;

	return modehandlers[mt][modeletter-65];
}

PrefixMode* ModeParser::FindPrefixMode(unsigned char modeletter)
{
	ModeHandler* mh = FindMode(modeletter, MODETYPE_CHANNEL);
	if (!mh)
		return NULL;
	return mh->IsPrefixMode();
}

PrefixMode* ModeParser::FindNearestPrefixMode(unsigned int rank)
{
	PrefixMode* pm = NULL;
	const PrefixModeList& prefixmodes = GetPrefixModes();
	for (PrefixModeList::const_iterator i = prefixmodes.begin(); i != prefixmodes.end(); ++i)
	{
		PrefixMode* thispm = *i;
		if (thispm->GetPrefixRank() < rank)
			continue; // Not ranked high enough.

		// Is it lower than the last checked mode?
		if (!pm || thispm->GetPrefixRank() < pm->GetPrefixRank())
			pm = thispm;

	}
	return pm;
}

PrefixMode* ModeParser::FindPrefix(unsigned const char pfxletter)
{
	const PrefixModeList& list = GetPrefixModes();
	for (PrefixModeList::const_iterator i = list.begin(); i != list.end(); ++i)
	{
		PrefixMode* pm = *i;
		if (pm->GetPrefix() == pfxletter)
			return pm;
	}
	return NULL;
}

std::string ModeParser::GiveModeList(ModeType mt)
{
	std::string type1;	/* Listmodes EXCEPT those with a prefix */
	std::string type2;	/* Modes that take a param when adding or removing */
	std::string type3;	/* Modes that only take a param when adding */
	std::string type4;	/* Modes that dont take a param */

	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		ModeHandler* mh = modehandlers[mt][mode-65];
		if (mh)
		{
			/* One parameter when adding */
			if (mh->NeedsParam(true))
			{
				PrefixMode* pm = mh->IsPrefixMode();
				if ((mh->IsListMode()) && ((!pm) || (pm->GetPrefix() == 0)))
				{
					type1 += mh->GetModeChar();
				}
				else
				{
					/* ... and one parameter when removing */
					if (mh->NeedsParam(false))
					{
						/* But not a list mode */
						if (!pm)
						{
							type2 += mh->GetModeChar();
						}
					}
					else
					{
						/* No parameters when removing */
						type3 += mh->GetModeChar();
					}
				}
			}
			else
			{
				type4 += mh->GetModeChar();
			}
		}
	}

	return type1 + "," + type2 + "," + type3 + "," + type4;
}

struct PrefixModeSorter
{
	bool operator()(PrefixMode* lhs, PrefixMode* rhs)
	{
		return lhs->GetPrefixRank() < rhs->GetPrefixRank();
	}
};

std::string ModeParser::BuildPrefixes(bool lettersAndModes)
{
	std::string mletters;
	std::string mprefixes;
	std::vector<PrefixMode*> prefixes;

	const PrefixModeList& list = GetPrefixModes();
	for (PrefixModeList::const_iterator i = list.begin(); i != list.end(); ++i)
	{
		PrefixMode* pm = *i;
		if (pm->GetPrefix())
			prefixes.push_back(pm);
	}

	std::sort(prefixes.begin(), prefixes.end(), PrefixModeSorter());
	for (std::vector<PrefixMode*>::const_reverse_iterator n = prefixes.rbegin(); n != prefixes.rend(); ++n)
	{
		mletters += (*n)->GetPrefix();
		mprefixes += (*n)->GetModeChar();
	}

	return lettersAndModes ? "(" + mprefixes + ")" + mletters : mletters;
}

void ModeParser::AddModeWatcher(ModeWatcher* mw)
{
	modewatchermap.insert(std::make_pair(mw->GetModeName(), mw));
}

bool ModeParser::DelModeWatcher(ModeWatcher* mw)
{
	std::pair<ModeWatcherMap::iterator, ModeWatcherMap::iterator> itpair = modewatchermap.equal_range(mw->GetModeName());
	for (ModeWatcherMap::iterator i = itpair.first; i != itpair.second; ++i)
	{
		if (i->second == mw)
		{
			modewatchermap.erase(i);
			return true;
		}
	}

	return false;
}

void ModeHandler::RemoveMode(User* user)
{
	// Remove the mode if it's set on the user
	if (user->IsModeSet(this))
	{
		Modes::ChangeList changelist;
		changelist.push_remove(this);
		ServerInstance->Modes->Process(ServerInstance->FakeClient, NULL, user, changelist, ModeParser::MODE_LOCALONLY);
	}
}

void ModeHandler::RemoveMode(Channel* channel, Modes::ChangeList& changelist)
{
	if (channel->IsModeSet(this))
	{
		if (this->NeedsParam(false))
			// Removing this mode requires a parameter
			changelist.push_remove(this, channel->GetModeParameter(this));
		else
			changelist.push_remove(this);
	}
}

void PrefixMode::RemoveMode(Channel* chan, Modes::ChangeList& changelist)
{
	const Channel::MemberMap& userlist = chan->GetUsers();
	for (Channel::MemberMap::const_iterator i = userlist.begin(); i != userlist.end(); ++i)
	{
		if (i->second->HasMode(this))
			changelist.push_remove(this, i->first->nick);
	}
}

bool ModeParser::IsModeChar(char chr)
{
	return ((chr >= 'A' && chr <= 'Z') || (chr >= 'a' && chr <= 'z'));
}

ModeParser::ModeParser()
{
	/* Clear mode handler list */
	memset(modehandlers, 0, sizeof(modehandlers));
	memset(modehandlersbyid, 0, sizeof(modehandlersbyid));
}

ModeParser::~ModeParser()
{
}
