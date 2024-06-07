/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2017-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Shawn Smith <ShawnSmith0828@gmail.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
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
#include "clientprotocolevent.h"
#include "numerichelper.h"

ModeHandler::ModeHandler(Module* Creator, const std::string& Name, char modeletter, ParamSpec Params, ModeType type, Class mclass)
	: ServiceProvider(Creator, Name, SERVICE_MODE)
	, modeid(ModeParser::MODEID_MAX)
	, parameters_taken(Params)
	, mode(ServerInstance->Config->ConfValue("modes")->getCharacter(Name, modeletter))
	, m_type(type)
	, type_id(mclass)
{
}

Cullable::Result ModeHandler::Cull()
{
	if (ServerInstance)
		ServerInstance->Modes.DelMode(this);
	return Cullable::Cull();
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

ModResult ModeHandler::AccessCheck(User*, Channel*, Modes::Change&)
{
	return MOD_RES_PASSTHRU;
}

bool ModeHandler::OnModeChange(User*, User*, Channel*, Modes::Change&)
{
	return false;
}

void ModeHandler::DisplayList(User*, Channel*)
{
}

void ModeHandler::DisplayEmptyList(User*, Channel*)
{
}

void ModeHandler::OnParameterMissing(User* user, User* dest, Channel* channel)
{
	std::string message = fmt::format("You must specify a parameter for the {} mode.", name);
	if (!syntax.empty())
		message.append(fmt::format(" Syntax: {}.", syntax));

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

bool ModeHandler::ResolveModeConflict(const std::string& theirs, const std::string& ours, Channel*)
{
	return (theirs < ours);
}

void ModeHandler::RegisterService()
{
	ServerInstance->Modes.AddMode(this);
	ServerInstance->Modules.AddReferent((GetModeType() == MODETYPE_CHANNEL ? "mode/" : "umode/") + name, this);
}

bool SimpleUserMode::OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change)
{
	/* We're either trying to add a mode we already have or
		remove a mode we don't have, deny. */
	if (dest->IsModeSet(this) == change.adding)
		return false;

	/* adding will be either true or false, depending on if we
		are adding or removing the mode, since we already checked
		to make sure we aren't adding a mode we have or that we
		aren't removing a mode we don't have, we don't have to do any
		other checks here to see if it's true or false, just add or
		remove the mode */
	dest->SetMode(this, change.adding);

	return true;
}

bool SimpleChannelMode::OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change)
{
	/* We're either trying to add a mode we already have or
		remove a mode we don't have, deny. */
	if (channel->IsModeSet(this) == change.adding)
		return false;

	/* adding will be either true or false, depending on if we
		are adding or removing the mode, since we already checked
		to make sure we aren't adding a mode we have or that we
		aren't removing a mode we don't have, we don't have to do any
		other checks here to see if it's true or false, just add or
		remove the mode */
	channel->SetMode(this, change.adding);

	return true;
}

ModeWatcher::ModeWatcher(Module* Creator, const std::string& modename, ModeType type)
	: mode(modename)
	, m_type(type)
	, creator(Creator)
{
	ServerInstance->Modes.AddModeWatcher(this);
}

ModeWatcher::~ModeWatcher()
{
	ServerInstance->Modes.DelModeWatcher(this);
}

bool ModeWatcher::BeforeMode(User*, User*, Channel*, Modes::Change&)
{
	return true;
}

void ModeWatcher::AfterMode(User*, User*, Channel*, const Modes::Change&)
{
}

PrefixMode::PrefixMode(Module* Creator, const std::string& Name, char ModeLetter, Rank PrefixRank, char PrefixChar)
	: ModeHandler(Creator, Name, ModeLetter, PARAM_ALWAYS, MODETYPE_CHANNEL, MC_PREFIX)
	, prefix(PrefixChar)
	, prefixrank(PrefixRank)
{
	list = true;
	syntax = "<nick>";
}

ModResult PrefixMode::AccessCheck(User* src, Channel*, Modes::Change& change)
{
	if (!change.adding && src->nick == change.param && selfremove)
		return MOD_RES_ALLOW;
	return MOD_RES_PASSTHRU;
}

bool PrefixMode::OnModeChange(User* source, User*, Channel* chan, Modes::Change& change)
{
	User* target;
	if (IS_LOCAL(source))
		target = ServerInstance->Users.FindNick(change.param);
	else
		target = ServerInstance->Users.Find(change.param);

	if (!target)
	{
		source->WriteNumeric(Numerics::NoSuchNick(change.param));
		return false;
	}

	Membership* memb = chan->GetUser(target);
	if (!memb)
	{
		source->WriteNumeric(ERR_USERNOTINCHANNEL, target->nick, chan->name, "They are not on that channel");
		return false;
	}

	change.param = target->nick;
	return memb->SetPrefix(this, change.adding);
}

void PrefixMode::Update(ModeHandler::Rank rank, ModeHandler::Rank setrank, ModeHandler::Rank unsetrank, bool selfrm)
{
	prefixrank = rank;
	ranktoset = setrank;
	ranktounset = unsetrank;
	selfremove = selfrm;
}

bool ParamModeBase::OnModeChange(User* source, User*, Channel* chan, Modes::Change& change)
{
	if (change.adding)
	{
		if (chan->GetModeParameter(this) == change.param)
			return false;

		if (!OnSet(source, chan, change.param))
			return false;

		chan->SetMode(this, true);

		// Handler might have changed the parameter internally
		change.param.clear();
		this->GetParameter(chan, change.param);
	}
	else
	{
		if (!chan->IsModeSet(this))
			return false;
		this->OnUnsetInternal(source, chan);
		chan->SetMode(this, false);
	}
	return true;
}

bool ModeParser::TryMode(User* user, User* targetuser, Channel* chan, Modes::Change& mcitem, bool SkipACL)
{
	ModeType type = chan ? MODETYPE_CHANNEL : MODETYPE_USER;

	ModeHandler* mh = mcitem.mh;
	const bool needs_param = mh->NeedsParam(mcitem.adding);

	// crop mode parameter size to MODE_PARAM_MAX characters
	if (mcitem.param.length() > MODE_PARAM_MAX && mcitem.adding)
		mcitem.param.erase(MODE_PARAM_MAX);

	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnRawMode, MOD_RESULT, (user, chan, mcitem));

	if (IS_LOCAL(user) && (MOD_RESULT == MOD_RES_DENY))
		return false;

	const char modechar = mh->GetModeChar();

	if (chan && !SkipACL && (MOD_RESULT != MOD_RES_ALLOW))
	{
		MOD_RESULT = mh->AccessCheck(user, chan, mcitem);

		if (MOD_RESULT == MOD_RES_DENY)
			return false;
		if (MOD_RESULT == MOD_RES_PASSTHRU)
		{
			ModeHandler::Rank neededrank = mh->GetLevelRequired(mcitem.adding);
			ModeHandler::Rank ourrank = chan->GetPrefixValue(user);
			if (ourrank < neededrank)
			{
				user->WriteNumeric(Numerics::ChannelPrivilegesNeeded(chan, neededrank, fmt::format("{} channel mode {} ({})",
					mcitem.adding ? "set" : "unset", mh->GetModeChar(), mh->name)));
				return false;
			}
		}
	}

	// Ask mode watchers whether this mode change is OK
	for (const auto& [_, mw] : insp::equal_range(modewatchermap, mh->name))
	{
		if (mw->GetModeType() == type)
		{
			if (!mw->BeforeMode(user, targetuser, chan, mcitem))
				return false;

			// A module whacked the parameter completely, and there was one. Abort.
			if ((needs_param) && (mcitem.param.empty()))
				return false;
		}
	}

	if ((chan || (!chan && mcitem.adding)) && IS_LOCAL(user) && mh->NeedsOper() && !user->HasModePermission(mh))
	{
		/* It's an oper only mode, and they don't have access to it. */
		if (user->IsOper())
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, fmt::format("Permission Denied - Oper type {} does not have access to {} {} mode {}",
				user->oper->GetType(), mcitem.adding ? "set" : "unset", type == MODETYPE_CHANNEL ? "channel" : "user", modechar));
		}
		else
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, fmt::format("Permission Denied - Only operators may {} {} mode {}",
				mcitem.adding ? "set" : "unset", type == MODETYPE_CHANNEL ? "channel" : "user", modechar));
		}
		return false;
	}

	/* Call the handler for the mode */
	if (!mh->OnModeChange(user, targetuser, chan, mcitem))
		return false;

	if ((needs_param) && (mcitem.param.empty()))
		return false;

	for (const auto& [_, mw] : insp::equal_range(modewatchermap, mh->name))
	{
		if (mw->GetModeType() == type)
			mw->AfterMode(user, targetuser, chan, mcitem);
	}

	return true;
}

void ModeParser::ModeParamsToChangeList(User* user, ModeType type, const std::vector<std::string>& parameters, Modes::ChangeList& changelist, size_t beginindex, size_t endindex)
{
	if (endindex > parameters.size())
		endindex = parameters.size();

	bool adding = true;
	size_t param_at = beginindex+1;

	for (const auto modechar : parameters[beginindex])
	{
		if (modechar == '+' || modechar == '-')
		{
			adding = (modechar == '+');
			continue;
		}

		ModeHandler* mh = this->FindMode(modechar, type);
		if (!mh)
		{
			/* No mode handler? Unknown mode character then. */
			int numeric = (type == MODETYPE_CHANNEL ? ERR_UNKNOWNMODE : ERR_UNKNOWNSNOMASK);
			const char* typestr = (type == MODETYPE_CHANNEL ? "channel" : "user");
			user->WriteNumeric(numeric, modechar, fmt::format("is not a recognised {} mode.", typestr));
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
static bool ShouldApplyMergedMode(Channel* chan, const Modes::Change& item)
{
	ModeHandler* mh = item.mh;
	if ((!chan) || (!chan->IsModeSet(mh)) || (mh->IsListMode()))
		// Mode not set here or merge is not applicable, apply the incoming mode
		return true;

	// Mode handler decides
	const std::string ours = chan->GetModeParameter(mh);
	return mh->ResolveModeConflict(item.param, ours, chan);
}

void ModeParser::Process(User* user, Channel* targetchannel, User* targetuser, Modes::ChangeList& changelist, ModeProcessFlag flags)
{
	// Call ProcessSingle until the entire list is processed, but at least once to ensure
	// LastParse and LastChangeList are cleared
	size_t processed = 0;
	do
	{
		processed += ProcessSingle(user, targetchannel, targetuser, changelist, flags, processed);
	}
	while (processed < changelist.size());
}

size_t ModeParser::ProcessSingle(User* user, Channel* targetchannel, User* targetuser, Modes::ChangeList& changelist, ModeProcessFlag flags, size_t beginindex)
{
	LastChangeList.clear();

	size_t modes_processed = 0;
	Modes::ChangeList::List& list = changelist.getlist();

	for (auto& item : insp::iterator_range(list.begin() + beginindex, list.end()))
	{
		modes_processed++;
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

		if (!TryMode(user, targetuser, targetchannel, item, (!(flags & MODE_CHECKACCESS))))
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
		Modes::Change modechange(mh, true, "");
		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnRawMode, MOD_RESULT, (user, chan, modechange));
		if (MOD_RESULT == MOD_RES_DENY)
			return;

		bool display = true;

		// Ask mode watchers whether it's OK to show the list
		for (const auto& [_, mw] : insp::equal_range(modewatchermap, mh->name))
		{
			if (mw->GetModeType() == MODETYPE_CHANNEL)
			{
				if (!mw->BeforeMode(user, nullptr, chan, modechange))
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

void ModeParser::CleanMask(std::string& mask)
{
	auto pos_of_pling    = mask.find_first_of('!');
	auto pos_of_at       = mask.find_first_of('@',  pos_of_pling == std::string::npos ? 0 : pos_of_pling);
	auto pos_of_hostchar = mask.find_first_of(":.", pos_of_at    == std::string::npos ? 0 : pos_of_at);

	if (pos_of_pling == mask.length()-1 || pos_of_at == mask.length()-1)
	{
		// Malformed mask; needs * after the ! or @.
		mask.append("*");
	}

	if (pos_of_pling == 0 || pos_of_at == 0)
	{
		// Malformed mask; needs * before the ! or @.
		mask.insert(0, "*");
	}

	if (pos_of_pling == std::string::npos && pos_of_at == std::string::npos)
	{
		if (pos_of_hostchar == std::string::npos)
			mask.append("!*@*"); // The mask looks like "nick".
		else
			mask.insert(0, "*!*@"); // The mask looks like "host".
	}
	else if (pos_of_pling == std::string::npos && pos_of_at != std::string::npos)
	{
		// The mask looks like "user@host".
		mask.insert(0, "*!");

	}
	else if (pos_of_pling != std::string::npos && pos_of_at == std::string::npos)
	{
		// The mask looks like "nick!user".
		mask.append("@*");
	}
	else if (pos_of_at-pos_of_pling == 1)
	{
		// The mask looks like "nick!@host".
		mask.insert(pos_of_at, "*");
	}
}

ModeHandler::Id ModeParser::AllocateModeId(ModeHandler* mh)
{
	for (ModeHandler::Id i = 0; i != MODEID_MAX; ++i)
	{
		if (!modehandlersbyid[mh->GetModeType()][i])
			return i;
	}

	throw ModuleException(mh->creator, "Out of mode ids");
}

void ModeParser::AddMode(ModeHandler* mh)
{
	if (!ModeParser::IsModeChar(mh->GetModeChar()))
		throw ModuleException(mh->creator, fmt::format("Mode letter for {} is invalid: {}", mh->name, mh->GetModeChar()));

	/* A mode prefix of ',' is not acceptable, it would fuck up server to server.
	 * A mode prefix of ':' will fuck up both server to server, and client to server.
	 * A mode prefix of '#' will mess up /whois and /privmsg
	 */
	PrefixMode* pm = mh->IsPrefixMode();
	if (pm)
	{
		if ((pm->GetPrefix() > 126) || (pm->GetPrefix() == ',') || (pm->GetPrefix() == ':') || ServerInstance->Channels.IsPrefix(pm->GetPrefix()))
			throw ModuleException(mh->creator, fmt::format("Mode prefix for {} is invalid: {}", mh->name, pm->GetPrefix()));

		PrefixMode* otherpm = FindPrefix(pm->GetPrefix());
		if (otherpm)
		{
			throw ModuleException(mh->creator, fmt::format("Mode prefix for {} already used by {} from {}: {}",
				mh->name, otherpm->name, otherpm->creator->ModuleFile, pm->GetPrefix()));
		}
	}

	ModeHandler*& slot = modehandlers[mh->GetModeType()][ModeParser::GetModeIndex(mh->GetModeChar())];
	if (slot)
	{
		throw ModuleException(mh->creator, fmt::format("Mode letter for {} already used by {} from {}: {}",
			mh->name, slot->name, slot->creator->ModuleFile, mh->GetModeChar()));
	}

	// The mode needs an id if it is either a user mode, a simple mode (flag) or a parameter mode.
	// Otherwise (for listmodes and prefix modes) the id remains MODEID_MAX, which is invalid.
	ModeHandler::Id modeid = MODEID_MAX;
	if ((mh->GetModeType() == MODETYPE_USER) || (mh->IsParameterMode()) || (!mh->IsListMode()))
		modeid = AllocateModeId(mh);

	std::pair<ModeHandlerMap::iterator, bool> res = modehandlersbyname[mh->GetModeType()].emplace(mh->name, mh);
	if (!res.second)
	{
		ModeHandler* othermh = res.first->second;
		throw ModuleException(mh->creator, fmt::format("Mode name {} already used by {} from {}",
			mh->name, othermh->GetModeChar(), othermh->creator->ModuleFile));
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

	ModeHandler*& slot = modehandlers[mh->GetModeType()][ModeParser::GetModeIndex(mh->GetModeChar())];
	if (slot != mh)
		return false;

	/* Note: We can't stack here, as we have modes potentially being removed across many different channels.
	 * To stack here we have to make the algorithm slower. Discuss.
	 */
	switch (mh->GetModeType())
	{
		case MODETYPE_USER:
		{
			const UserMap& users = ServerInstance->Users.GetUsers();
			for (UserMap::const_iterator i = users.begin(); i != users.end(); )
			{
				User* user = i->second;
				++i;
				mh->RemoveMode(user);
			}
		}
		break;
		case MODETYPE_CHANNEL:
		{
			const ChannelMap& chans = ServerInstance->Channels.GetChans();
			for (ChannelMap::const_iterator i = chans.begin(); i != chans.end(); )
			{
				// The channel may not be in the hash after RemoveMode(), see m_permchannels
				Channel* chan = i->second;
				++i;

				Modes::ChangeList changelist;
				mh->RemoveMode(chan, changelist);
				this->Process(ServerInstance->FakeClient, chan, nullptr, changelist, MODE_LOCALONLY);
			}
		}
		break;
	}

	mhmap.erase(mhmapit);
	if (mh->GetId() != MODEID_MAX)
		modehandlersbyid[mh->GetModeType()][mh->GetId()] = nullptr;
	slot = nullptr;
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

	return nullptr;
}

ModeHandler* ModeParser::FindMode(unsigned char modeletter, ModeType mt)
{
	if (!ModeParser::IsModeChar(modeletter))
		return nullptr;

	return modehandlers[mt][ModeParser::GetModeIndex(modeletter)];
}

PrefixMode* ModeParser::FindPrefixMode(unsigned char modeletter)
{
	ModeHandler* mh = FindMode(modeletter, MODETYPE_CHANNEL);
	if (!mh)
		return nullptr;
	return mh->IsPrefixMode();
}


PrefixMode* ModeParser::FindNearestPrefixMode(ModeHandler::Rank rank) const
{
	PrefixMode* pm = nullptr;
	for (auto* thispm : GetPrefixModes())
	{
		if (thispm->GetPrefixRank() < rank)
			continue; // Not ranked high enough.

		// Is it lower than the last checked mode?
		if (!pm || thispm->GetPrefixRank() < pm->GetPrefixRank())
			pm = thispm;

	}
	return pm;
}

PrefixMode* ModeParser::FindPrefix(unsigned char pfxletter) const
{
	for (auto* pm : GetPrefixModes())
	{
		if (pm->GetPrefix() == pfxletter)
			return pm;
	}
	return nullptr;
}

void ModeParser::AddModeWatcher(ModeWatcher* mw)
{
	modewatchermap.emplace(mw->GetModeName(), mw);
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
		ServerInstance->Modes.Process(ServerInstance->FakeClient, nullptr, user, changelist, ModeParser::MODE_LOCALONLY);
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
	for (const auto& [user, memb] : chan->GetUsers())
	{
		if (memb->HasMode(this))
			changelist.push_remove(this, user->nick);
	}
}

bool ModeParser::IsModeChar(char chr)
{
	return ((chr >= '0' && chr <= '9') || (chr >= 'A' && chr <= 'Z') || (chr >= 'a' && chr <= 'z'));
}

size_t ModeParser::GetModeIndex(char chr)
{
	// Bitset layout:
	//   0123456789                 = 10 [0-9]
	//   ABCDEFGHIJKLMNOPQRSTUVWXYZ = 26 [10-35]
	//   abcdefghijklmnopqrstuvwxyz = 26 [36-61]
	if (chr >= '0' && chr <= '9')
		return chr - '0';

	if (chr >= 'A' && chr <= 'Z')
		return chr - 'A' + 10; // [0-9] = 10

	if (chr >= 'a' && chr <= 'z')
		return chr - 'a' + 36; // [0-9]+[A-Z] = 10+26 = 36

	return ModeParser::MODEID_MAX;
}

ModeParser::ModeParser()
{
	/* Clear mode handler list */
	memset(modehandlers, 0, sizeof(modehandlers));
	memset(modehandlersbyid, 0, sizeof(modehandlersbyid));
}
