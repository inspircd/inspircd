/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "inspstring.h"

/* +s (secret) */
#include "modes/cmode_s.h"
/* +p (private) */
#include "modes/cmode_p.h"
/* +b (bans) */
#include "modes/cmode_b.h"
/* +m (moderated) */
#include "modes/cmode_m.h"
/* +t (only (half) ops can change topic) */
#include "modes/cmode_t.h"
/* +n (no external messages) */
#include "modes/cmode_n.h"
/* +i (invite only) */
#include "modes/cmode_i.h"
/* +k (keyed channel) */
#include "modes/cmode_k.h"
/* +l (channel user limit) */
#include "modes/cmode_l.h"
/* +o (channel op) */
#include "modes/cmode_o.h"
/* +v (channel voice) */
#include "modes/cmode_v.h"
/* +w (see wallops) */
#include "modes/umode_w.h"
/* +i (invisible) */
#include "modes/umode_i.h"
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
	if (ServerInstance && ServerInstance->Modes && ServerInstance->Modes->FindMode(mode, m_type) == this)
		ServerInstance->Logs->Log("MODE", DEFAULT, "ERROR: Destructor for mode %c called while still registered", mode);
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
	if (adding)
	{
		if (!dest->IsModeSet(this->GetModeChar()))
		{
			dest->SetMode(this->GetModeChar(),true);
			return MODEACTION_ALLOW;
		}
	}
	else
	{
		if (dest->IsModeSet(this->GetModeChar()))
		{
			dest->SetMode(this->GetModeChar(),false);
			return MODEACTION_ALLOW;
		}
	}

	return MODEACTION_DENY;
}


ModeAction SimpleChannelModeHandler::OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
{
	if (adding)
	{
		if (!channel->IsModeSet(this->GetModeChar()))
		{
			channel->SetMode(this->GetModeChar(),true);
			return MODEACTION_ALLOW;
		}
	}
	else
	{
		if (channel->IsModeSet(this->GetModeChar()))
		{
			channel->SetMode(this->GetModeChar(),false);
			return MODEACTION_ALLOW;
		}
	}

	return MODEACTION_DENY;
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

irc::modechange::modechange(const std::string& name, const std::string& param, bool add)
	: adding(add), value(param)
{
	ModeHandler* mh = ServerInstance->Modes->FindMode(name);
	if (mh)
		mode = mh->id;
}

irc::modechange::modechange(char modechar, ModeType type, const std::string& param, bool add)
{
	ModeHandler* mh = ServerInstance->Modes->FindMode(modechar, type);
	if (mh)
		mode = mh->id;
}

std::string irc::modestacker::popModeLine(bool use_uid)
{
	char pm_now = '\0';
	std::string modeline;
	std::stringstream params;
	unsigned int nmodes = 0, linelen = 0;

	std::vector<modechange>::iterator iter = sequence.begin();
	for(; iter != sequence.end() && nmodes < ServerInstance->Config->Limits.MaxModes; ++iter)
	{
		char pm = iter->adding ? '+' : '-';
		ModeHandler* mh = ServerInstance->Modes->FindMode(iter->mode);
		if (!mh)
			continue;
		char modechar = mh->GetModeChar();
		std::string value = iter->value;
		if (mh->GetTranslateType() == TR_NICK)
		{
			User* u = ServerInstance->FindNick(value);
			if (u)
				value = use_uid ? u->uuid : u->nick;
		}

		if (!modechar)
		{
			modechar = 'Z';
			if (mh->GetNumParams(iter->adding))
				value = mh->name + "=" + value;
			else
				value = mh->name;
		}
		else if (mh->GetNumParams(iter->adding) && value.empty())
		{
			// value is empty when we want a param
			continue;
		}
		else if (!mh->GetNumParams(iter->adding))
			value.clear();

		int mylen = (pm == pm_now) ? 1 : 2;
		if (!value.empty())
			mylen += 1 + value.length();

		if (mylen + linelen + 100 > MAXBUF)
			goto line_full;

		linelen += mylen;
		if (pm != pm_now)
		{
			modeline.push_back(pm);
			pm_now = pm;
		}
		modeline.push_back(modechar);

		if (!value.empty())
			params << " " << value;
	}

line_full:
	sequence.erase(sequence.begin(), iter);
	return modeline + params.str();
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

ModeAction ModeParser::TryMode(User* user, User* targetuser, Channel* chan, irc::modechange& mc, bool SkipACL)
{
	bool adding = mc.adding;
	ModeID modeid = mc.mode;
	ModeType type = chan ? MODETYPE_CHANNEL : MODETYPE_USER;

	ModeHandler *mh = FindMode(modeid);
	int pcnt = mh->GetNumParams(adding);

	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnRawMode, MOD_RESULT, (user, chan, mc));

	if (IS_LOCAL(user) && (MOD_RESULT == MOD_RES_DENY))
		return MODEACTION_DENY;

	if (chan && !SkipACL && (MOD_RESULT != MOD_RES_ALLOW))
	{
		MOD_RESULT = mh->AccessCheck(user, chan, mc.value, adding);

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
				for(ModeIDIter id; id; id++)
				{
					ModeHandler* privmh = FindMode(id);
					if (privmh && privmh->GetPrefixRank() >= neededrank)
					{
						// this mode is sufficient to allow this action
						if (!neededmh || privmh->GetPrefixRank() < neededmh->GetPrefixRank())
							neededmh = privmh;
					}
				}
				if (neededmh)
					user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must have channel %s access or above to %sset the %s channel mode",
						user->nick.c_str(), chan->name.c_str(), neededmh->name.c_str(), adding ? "" : "un", mh->name.c_str());
				else
					user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You cannot %sset the %s channel mode",
						user->nick.c_str(), chan->name.c_str(), adding ? "" : "un", mh->name.c_str());
				return MODEACTION_DENY;
			}
		}
	}

	std::pair<ModeWatcherMap::iterator,ModeWatcherMap::iterator> watchers = modewatchers.equal_range(modeid);
	for (ModeWatcherMap::iterator watcher = watchers.first; watcher != watchers.second; watcher++)
	{
		if (watcher->second->BeforeMode(user, targetuser, chan, mc.value, adding, type) == false)
			return MODEACTION_DENY;
		/* A module whacked the parameter completely, and there was one. abort. */
		if (pcnt && mc.value.empty())
			return MODEACTION_DENY;
	}

	if (IS_LOCAL(user) && !IS_OPER(user))
	{
		char* disabled = (type == MODETYPE_CHANNEL) ? ServerInstance->Config->DisabledCModes : ServerInstance->Config->DisabledUModes;
		if (disabled[mh->GetModeChar() - 'A'])
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - %s mode %c has been locked by the administrator",
				user->nick.c_str(), type == MODETYPE_CHANNEL ? "channel" : "user", mh->GetModeChar());
			return MODEACTION_DENY;
		}
	}

	if (adding && IS_LOCAL(user) && mh->NeedsOper() && !user->HasModePermission(mh->GetModeChar(), type))
	{
		/* It's an oper only mode, and they don't have access to it. */
		if (IS_OPER(user))
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - Oper type %s does not have access to set %s mode %c",
					user->nick.c_str(), user->oper->NameStr(), type == MODETYPE_CHANNEL ? "channel" : "user", mh->GetModeChar());
		}
		else
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - Only operators may set %s mode %c",
					user->nick.c_str(), type == MODETYPE_CHANNEL ? "channel" : "user", mh->GetModeChar());
		}
		return MODEACTION_DENY;
	}

	if (mh->GetTranslateType() == TR_NICK && !ServerInstance->FindNick(mc.value))
	{
		user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), mc.value.c_str());
		return MODEACTION_DENY;
	}

	if (mh->GetPrefixRank() && chan)
	{
		User* user_to_prefix = ServerInstance->FindNick(mc.value);
		if (!user_to_prefix)
			return MODEACTION_DENY;
		if (!chan->SetPrefix(user_to_prefix, mh->GetModeChar(), adding))
			return MODEACTION_DENY;
	}

	/* Call the handler for the mode */
	ModeAction ma = mh->OnModeChange(user, targetuser, chan, mc.value, adding);

	if (pcnt && mc.value.empty())
		return MODEACTION_DENY;

	if (ma != MODEACTION_ALLOW)
		return ma;

	for (ModeWatcherMap::iterator watcher = watchers.first; watcher != watchers.second; watcher++)
		watcher->second->AfterMode(user, targetuser, chan, mc.value, adding, type);

	return MODEACTION_ALLOW;
}

void ModeParser::Parse(const std::vector<std::string>& parameters, User *user, Extensible*& target, irc::modestacker& modes)
{
	std::string targetstr = parameters[0];
	Channel* targetchannel = ServerInstance->FindChan(targetstr);
	User* targetuser = ServerInstance->FindNick(targetstr);
	ModeType type = targetchannel ? MODETYPE_CHANNEL : MODETYPE_USER;

	if (!targetchannel && !targetuser)
	{
		user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",user->nick.c_str(),targetstr.c_str());
		return;
	}
	if (parameters.size() == 1)
	{
		this->DisplayCurrentModes(user, targetuser, targetchannel, targetstr.c_str());
		return;
	}
	if (targetchannel && parameters.size() == 2)
	{
		/* Special case for displaying the list for listmodes,
		 * e.g. MODE #chan b, or MODE #chan +b without a parameter
		 */
		this->DisplayListModes(user, targetchannel, parameters[1]);
	}

	unsigned int param_at = 2;
	bool adding = true;

	for (std::string::const_iterator letter = parameters[1].begin(); letter != parameters[1].end(); letter++)
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

		std::string parameter = "";
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
		}
		modes.push(irc::modechange(mh->id, parameter, adding));
	}

	target = targetuser ? (Extensible*)targetuser : targetchannel;
}

void ModeParser::Send(User *src, Extensible* target, irc::modestacker modes)
{
	Channel* targetchannel = dynamic_cast<Channel*>(target);
	User* targetuser = dynamic_cast<User*>(target);
	if (targetchannel)
	{
		while (!modes.empty())
			targetchannel->WriteChannel(src, "MODE %s %s", targetchannel->name.c_str(), modes.popModeLine().c_str());
	}
	else
	{
		while (!modes.empty())
			targetuser->WriteFrom(src, "MODE %s %s", targetuser->nick.c_str(), modes.popModeLine().c_str());
	}
}

void ModeParser::Process(User *src, Extensible* target, irc::modestacker& modes, bool merge)
{
	Channel* targetchannel = dynamic_cast<Channel*>(target);
	User* targetuser = dynamic_cast<User*>(target);

	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnPreMode, MOD_RESULT, (src, target, modes));

	bool SkipAccessChecks = false;

	if (!IS_LOCAL(src) || MOD_RESULT == MOD_RES_ALLOW)
		SkipAccessChecks = true;
	else if (MOD_RESULT == MOD_RES_DENY)
		return;

	if (targetuser && src != targetuser && !SkipAccessChecks)
	{
		src->WriteNumeric(ERR_USERSDONTMATCH, "%s :Can't change mode for other users", src->nick.c_str());
		return;
	}

	std::vector<irc::modechange>::iterator mc = modes.sequence.begin();
	while (mc != modes.sequence.end())
	{
		ModeHandler* mh = FindMode(mc->mode);
		if (mc->value.c_str()[0] == ':' || mc->value.find(' ') != std::string::npos)
		{
			// you can't do that in a mode value, sorry
			mc = modes.sequence.erase(mc);
			continue;
		}
		if (merge && targetchannel && targetchannel->IsModeSet(mh) && !mh->IsListMode())
		{
			std::string ours = targetchannel->GetModeParameter(mh);
			if (!mh->ResolveModeConflict(mc->value, ours, targetchannel))
			{
				/* we won the mode merge, don't apply this mode */
				mc = modes.sequence.erase(mc);
				continue;
			}
		}

		ModeAction ma = TryMode(src, targetuser, targetchannel, *mc, SkipAccessChecks);

		if (ma == MODEACTION_ALLOW)
		{
			mc++;
		}
		else
		{
			/* mode change was denied */
			mc = modes.sequence.erase(mc);
			continue;
		}
	}
	
	FOREACH_MOD(I_OnMode, OnMode(src, target, modes));
}

void ModeParser::DisplayListModes(User* user, Channel* chan, const std::string &mode_sequence)
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

		irc::modechange mc(mh->id);
		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnRawMode, MOD_RESULT, (user, chan, mc));
		if (MOD_RESULT == MOD_RES_DENY)
			continue;

		bool display = true;
		if (!user->HasPrivPermission("channels/auspex") && ServerInstance->Config->HideModeLists[mletter] && (chan->GetPrefixValue(user) < HALFOP_VALUE))
		{
			user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You do not have access to view the +%c list",
				user->nick.c_str(), chan->name.c_str(), mletter);
			display = false;
		}

		std::pair<ModeWatcherMap::iterator,ModeWatcherMap::iterator> watchers = modewatchers.equal_range(mh->id);
		for(ModeWatcherMap::iterator watcher = watchers.first; watcher != watchers.second; watcher++)
		{
			std::string dummyparam;

			if (!watcher->second->BeforeMode(user, NULL, chan, dummyparam, true, MODETYPE_CHANNEL))
				display = false;
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

bool ModeParser::AddMode(ModeHandler* mh)
{
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

	if (FindMode(mh->GetModeChar(), mh->GetModeType()))
		return false;
	
	if (FindMode(mh->name))
		return false;
	
	for(int id = 1; id < MODE_ID_MAX; id++)
	{
		if (handlers[id])
			continue;
		mh->id.SetID(id);
		handlers[id] = mh;
		return true;
	}
	return false;
}

bool ModeParser::DelMode(ModeHandler* mh)
{
	if (handlers[mh->id.GetID()] != mh)
		return false;

	/* Note: We can't stack here, as we have modes potentially being removed across many different channels.
	 * To stack here we have to make the algorithm slower. Discuss.
	 */
	switch (mh->GetModeType())
	{
		case MODETYPE_USER:
			for (user_hash::iterator i = ServerInstance->Users->clientlist->begin(); i != ServerInstance->Users->clientlist->end(); i++)
			{
				mh->RemoveMode(i->second);
			}
		break;
		case MODETYPE_CHANNEL:
			for (chan_hash::iterator i = ServerInstance->chanlist->begin(); i != ServerInstance->chanlist->end(); i++)
			{
				mh->RemoveMode(i->second);
			}
		break;
	}

	handlers[mh->id.GetID()] = NULL;

	return true;
}

ModeHandler* ModeParser::FindMode(const std::string& name)
{
	for(int id = 1; id < MODE_ID_MAX; id++)
	{
		if (handlers[id] && handlers[id]->name == name)
			return handlers[id];
	}
	return NULL;
}

ModeHandler* ModeParser::FindMode(unsigned const char modeletter, ModeType mt)
{
	for(int id = 1; id < MODE_ID_MAX; id++)
	{
		if (handlers[id] && handlers[id]->GetModeChar() == modeletter && handlers[id]->GetModeType() == mt)
			return handlers[id];
	}
	return NULL;
}

std::string ModeParser::UserModeList()
{
	char modestr[256];
	int pointer = 0;

	for(ModeIDIter id; id; id++)
	{
		ModeHandler* mh = FindMode(id);
		if(mh && mh->GetModeType() == MODETYPE_USER)
			modestr[pointer++] = mh->GetModeChar();
	}
	std::sort(modestr, modestr + pointer);
	modestr[pointer++] = 0;
	return modestr;
}

std::string ModeParser::ChannelModeList()
{
	char modestr[256];
	int pointer = 0;

	for(ModeIDIter id; id; id++)
	{
		ModeHandler* mh = FindMode(id);
    	if (mh && mh->GetModeType() == MODETYPE_CHANNEL)
			modestr[pointer++] = mh->GetModeChar();
	}
	modestr[pointer] = 0;
	std::sort(modestr, modestr + pointer);
	return modestr;
}

std::string ModeParser::ParaModeList()
{
	char modestr[256];
	int pointer = 0;

	for(ModeIDIter id; id; id++)
	{
		ModeHandler* mh = FindMode(id);
    	if (mh && mh->GetModeType() == MODETYPE_CHANNEL && mh->GetNumParams(true))
			modestr[pointer++] = mh->GetModeChar();
	}
	modestr[pointer++] = 0;
	return modestr;
}

ModeHandler* ModeParser::FindPrefix(unsigned const char pfxletter)
{
	for(ModeIDIter id; id; id++)
	{
		ModeHandler* mh = FindMode(id);
		if (mh && mh->GetPrefix() == pfxletter)
			return mh;
	}
	return NULL;
}

std::string ModeParser::GiveModeList(ModeType m)
{
	std::string type1;	/* Listmodes EXCEPT those with a prefix */
	std::string type2;	/* Modes that take a param when adding or removing */
	std::string type3;	/* Modes that only take a param when adding */
	std::string type4;	/* Modes that dont take a param */

	for(ModeIDIter id; id; id++)
	{
		ModeHandler* mh = FindMode(id);
		if (mh && mh->GetModeType() == m)
		{
			if (mh->GetNumParams(true))
			{
				if ((mh->IsListMode()) && (!mh->GetPrefix()))
				{
					type1 += mh->GetModeChar();
				}
				else
				{
					/* ... and one parameter when removing */
					if (mh->GetNumParams(false))
					{
						/* But not a list mode */
						if (!mh->GetPrefix())
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

	std::sort(type1.begin(), type1.end());
	std::sort(type2.begin(), type2.end());
	std::sort(type3.begin(), type3.end());
	std::sort(type4.begin(), type4.end());
	return type1 + "," + type2 + "," + type3 + "," + type4;
}

std::string ModeParser::BuildPrefixes(bool lettersAndModes)
{
	std::string mletters;
	std::string mprefixes;
	std::map<int,std::pair<char,char> > prefixes;

	for(ModeIDIter id; id; id++)
	{
		ModeHandler* mh = FindMode(id);

   		if (mh && mh->GetPrefix())
		{
			prefixes[mh->GetPrefixRank()] = std::make_pair(mh->GetPrefix(), mh->GetModeChar());
		}
	}

	for(std::map<int,std::pair<char,char> >::reverse_iterator n = prefixes.rbegin(); n != prefixes.rend(); n++)
	{
		mletters = mletters + n->second.first;
		mprefixes = mprefixes + n->second.second;
	}

	return lettersAndModes ? "(" + mprefixes + ")" + mletters : mletters;
}

bool ModeParser::AddModeWatcher(ModeWatcher* mw)
{
	if (!mw)
		return false;

	ModeHandler* mh = FindMode(mw->GetModeChar(), mw->GetModeType());

	if (!mh)
		return false;

	modewatchers.insert(std::make_pair(mh->id,mw));

	return true;
}

bool ModeParser::DelModeWatcher(ModeWatcher* mw)
{
	if (!mw)
		return false;

	ModeHandler* mh = FindMode(mw->GetModeChar(), mw->GetModeType());

	if (!mh)
		return false;

	std::pair<ModeWatcherMap::iterator,ModeWatcherMap::iterator> watchers = modewatchers.equal_range(mh->id);
	
	while (watchers.first != watchers.second)
	{
		if (watchers.first->second == mw)
		{
			modewatchers.erase(watchers.first);
			return true;
		}
		watchers.first++;
	}

	return false;
}

/** This default implementation can remove simple user modes
 */
void ModeHandler::RemoveMode(User* user, irc::modestacker* stack)
{
	if (user->IsModeSet(this->GetModeChar()))
	{
		irc::modechange mc(id, "", false);
		if (stack)
		{
			stack->push(mc);
		}
		else
		{
			irc::modestacker tmp;
			tmp.push(mc);
			ServerInstance->SendMode(ServerInstance->FakeClient, user, tmp, false);
		}
	}
}

/** This default implementation can remove non-list modes
 */
void ModeHandler::RemoveMode(Channel* channel, irc::modestacker* stack)
{
	if (channel->IsModeSet(this))
	{
		irc::modechange mc(id, channel->GetModeParameter(this), false);
		if (stack)
		{
			stack->push(mc);
		}
		else
		{
			irc::modestacker ms;
			ms.push(mc);
			ServerInstance->SendMode(ServerInstance->FakeClient, channel, ms, false);
		}
	}
}

void ListModeBase::DisplayList(User* user, Channel* channel)
{
	modelist* el = extItem.get(channel);
	if (el)
	{
		for (modelist::reverse_iterator it = el->rbegin(); it != el->rend(); ++it)
		{
			user->WriteNumeric(listnumeric, "%s %s %s %s %s", user->nick.c_str(), channel->name.c_str(), it->mask.c_str(), (it->nick.length() ? it->nick.c_str() : ServerInstance->Config->ServerName.c_str()), it->time.c_str());
		}
	}
	user->WriteNumeric(endoflistnumeric, "%s %s :%s", user->nick.c_str(), channel->name.c_str(), endofliststring.c_str());
}

const modelist* ListModeBase::GetList(Channel* channel)
{
	return extItem.get(channel);
}


void ListModeBase::RemoveMode(Channel* channel, irc::modestacker* stack)
{
	modelist* el = extItem.get(channel);
	if (el)
	{
		irc::modestacker modestack;

		for (modelist::iterator it = el->begin(); it != el->end(); it++)
		{
			if (stack)
				stack->push(irc::modechange(id, it->mask, false));
			else
				modestack.push(irc::modechange(id, it->mask, false));
		}

		if (stack)
			return;

		ServerInstance->SendMode(ServerInstance->FakeClient, channel, modestack, false);
	}
}

void ListModeBase::RemoveMode(User*, irc::modestacker* stack)
{
	// listmodes don't get set on users
}

void ListModeBase::DoRehash()
{
	ConfigTagList tags = ServerInstance->Config->ConfTags(configtag);

	chanlimits.clear();

	for (ConfigIter i = tags.first; i != tags.second; i++)
	{
		// For each <banlist> tag
		ConfigTag* c = i->second;
		ListLimit limit;
		limit.mask = c->getString("chan");
		limit.limit = c->getInt("limit");

		if (limit.mask.size() && limit.limit > 0)
			chanlimits.push_back(limit);
	}
	if (chanlimits.size() == 0)
	{
		ListLimit limit;
		limit.mask = "*";
		limit.limit = 64;
		chanlimits.push_back(limit);
	}
}

ModeAction ListModeBase::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
{
	// Try and grab the list
	modelist* el = extItem.get(channel);

	if (adding)
	{
		// If there was no list
		if (!el)
		{
			// Make one
			el = new modelist;
			extItem.set(channel, el);
		}

		// Clean the mask up
		if (this->tidy)
			ModeParser::CleanMask(parameter);

		// Check if the item already exists in the list
		for (modelist::iterator it = el->begin(); it != el->end(); it++)
		{
			if (parameter == it->mask)
			{
				/* Give a subclass a chance to error about this */
				TellAlreadyOnList(source, channel, parameter);

				// it does, deny the change
				return MODEACTION_DENY;
			}
		}

		unsigned int maxsize = 0;

		for (limitlist::iterator it = chanlimits.begin(); it != chanlimits.end(); it++)
		{
			if (InspIRCd::Match(channel->name, it->mask))
			{
				// We have a pattern matching the channel...
				maxsize = el->size();
				if (IS_LOCAL(source) || (maxsize < it->limit))
				{
					/* Ok, it *could* be allowed, now give someone subclassing us
					 * a chance to validate the parameter.
					 * The param is passed by reference, so they can both modify it
					 * and tell us if we allow it or not.
					 *
					 * eg, the subclass could:
					 * 1) allow
					 * 2) 'fix' parameter and then allow
					 * 3) deny
					 */
					if (ValidateParam(source, channel, parameter))
					{
						// And now add the mask onto the list...
						ListItem e;
						e.mask = parameter;
						e.nick = source->nick;
						e.time = stringtime();

						el->push_back(e);
						return MODEACTION_ALLOW;
					}
					else
					{
						/* If they deny it they have the job of giving an error message */
						return MODEACTION_DENY;
					}
				}
			}
		}

		/* List is full, give subclass a chance to send a custom message */
		if (!TellListTooLong(source, channel, parameter))
		{
			source->WriteNumeric(478, "%s %s %s :Channel ban/ignore list is full", source->nick.c_str(), channel->name.c_str(), parameter.c_str());
		}

		parameter = "";
		return MODEACTION_DENY;
	}
	else
	{
		// We're taking the mode off
		if (el)
		{
			for (modelist::iterator it = el->begin(); it != el->end(); it++)
			{
				if (parameter == it->mask)
				{
					el->erase(it);
					if (el->size() == 0)
					{
						extItem.unset(channel);
					}
					return MODEACTION_ALLOW;
				}
			}
			/* Tried to remove something that wasn't set */
			TellNotSet(source, channel, parameter);
			parameter = "";
			return MODEACTION_DENY;
		}
		else
		{
			/* Hmm, taking an exception off a non-existant list, DIE */
			TellNotSet(source, channel, parameter);
			parameter = "";
			return MODEACTION_DENY;
		}
	}
	return MODEACTION_DENY;
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
		b.init();
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
	memset(handlers, 0, sizeof(handlers));

	seq = 0;
	memset(&sent, 0, sizeof(sent));

	static_modes.init(this);
}

ModeParser::~ModeParser()
{
}
