/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Adds timed modes */

#include "inspircd.h"
// This isn't a listmode, but we need this anyway for the limit of the number of timed modes allowed.
#include "u_listmode.h"

/** Holds a timed mode
 */
class TimedMode
{
 public:
	time_t expire;
	irc::modestacker modes;
	bool operator==(const TimedMode& other) const
	{
		if(expire != other.expire) return false;
		std::vector<irc::modechange>::const_iterator i = modes.sequence.begin(), j = other.modes.sequence.begin();
		for(; i != modes.sequence.end() && j != other.modes.sequence.end(); ++i, ++j)
			if(!(i->mode == j->mode && i->adding == j->adding && !i->value.compare(j->value))) return false;
		return true;
	}
};

typedef std::vector<TimedMode> timedmodes;

// XXX: do we want this implemented as operator< in irc::modechange?
// Note that equal modes are considered to be modes that would clobber each other.
// Example: +l 3, +l 5, and -l are equal, as are +b a!b@c and -b a!b@c, but +b a!b@c and -b d!e@f are not.
static bool modeChangeLessThan(irc::modechange n1, irc::modechange n2) {
	if(n1.mode < n2.mode) return true;
	if(n2.mode < n1.mode) return false;
	ModeHandler* mh = ServerInstance->Modes->FindMode(n1.mode);
	if(!mh->IsListMode()) return false;
	return n1.value.compare(n2.value) < 0;
}
/* If we do make the above irc::modechange::operator<, we may want to make the following operator==:
	if(n1.mode != n2.mode) return false;
	ModeHandler* mh = ServerInstance->Modes->FindMode(n1.mode);
	if(!mh->IsListMode()) return true;
	return !n1.value.compare(n2.value);
And possibly make the following operator<=:
	if(n1.mode < n2.mode) return true;
	if(n2.mode < n1.mode) return false;
	ModeHandler* mh = ServerInstance->Modes->FindMode(n1.mode);
	if(!mh->IsListMode()) return true;
	return n1.value.compare(n2.value) <= 0;
*/

/** Handle /TMODE
 */
class CommandTmode : public Command
{
 public:
	SimpleExtItem<timedmodes> tmodes;
	TimedMode* current;
	irc::modestacker toannounce;
	limitlist chanlimits;
	CommandTmode(Module* Creator) : Command(Creator,"TMODE", 1), tmodes("tmodes", Creator)
	{
		syntax = "<channel> [<duration> <modes> {<mode-parameters>}]"; // NB: This is the syntax for clients to use.  The s2s syntax is different: <channel> <expiry> <modes to set/unset upon expiry> {<mode-parameters>}
		ServerInstance->Extensions.Register(&tmodes);
		TRANSLATE4(TR_TEXT, TR_TEXT, TR_TEXT, TR_END);
	}

	bool isDuplicate(Channel* chan, const TimedMode &T)
	{
		timedmodes* existing = tmodes.get(chan);
		if(!existing) return false;
		for (timedmodes::iterator i = existing->begin(); i != existing->end(); ++i)
			if(T == *i) return true;
		return false;
	} 

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		if(IS_SERVER(user)) {
			// handle a server sending us a partially processed TMODE
			TimedMode T;
			Channel* channel = ServerInstance->FindChan(parameters[0]);
			if(!channel || parameters.size() < 3) return CMD_FAILURE;
			T.expire = atol(parameters[1].c_str());
			std::vector<std::string> tmpparams = parameters;
			tmpparams.erase(tmpparams.begin()+1); // Remove the expiry from the parameters we're giving to Parse()
			Extensible* tmp;
			ServerInstance->Modes->Parse(tmpparams, user, tmp, T.modes);
			if(T.modes.sequence.empty()) return CMD_SUCCESS;
			stable_sort(T.modes.sequence.begin(), T.modes.sequence.end(), &modeChangeLessThan);
			timedmodes* existing = tmodes.get(channel);
			if(!existing) {
				existing = new timedmodes();
				tmodes.set(channel, existing);
			}
			else { // if we just created the new container, it's obviously empty, so obviously we don't need to check for duplicates
				for (timedmodes::iterator i = existing->begin(); i != existing->end(); ++i)
					if(T == *i) return CMD_SUCCESS; // we already have this TimedMode, so this one's a duplicate
			}
			existing->push_back(T);
			return CMD_SUCCESS;
		} else {
			// handle a user sending /TMODE
			TimedMode T;
			Channel* channel = ServerInstance->FindChan(parameters[0]);
			long duration;
			if (!channel)
			{
				user->WriteNumeric(401, "%s %s :No such channel",user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}
			if (parameters.size() < 3)
			{
				timedmodes* existing = tmodes.get(channel);
				if(existing)
					for (timedmodes::iterator i = existing->begin(); i != existing->end(); ++i)
					{
						irc::modestacker modes = i->modes;
						std::string msgbegin = user->nick + " " + channel->name + " " + ConvToStr(i->expire) + " :";
						int maxModeLength = MAXBUF - 9 - ServerInstance->Config->ServerName.length() - msgbegin.length();
						while(!modes.sequence.empty()) {
							user->WriteNumeric(954, msgbegin + modes.popModeLine(FORMAT_USER, maxModeLength, INT_MAX));
						}
					}
				user->WriteNumeric(953, "%s %s :End of channel timed modes", user->nick.c_str(), channel->name.c_str());
				return CMD_SUCCESS;
			}
			unsigned int maxsize = 30;
			for (limitlist::iterator it = chanlimits.begin(); it != chanlimits.end(); ++it)
				if (InspIRCd::Match(channel->name, it->mask)) {
					maxsize = it->limit;
					break;
				}
			timedmodes* existing = tmodes.get(channel);
			if ((existing && existing->size() >= maxsize) || maxsize == 0)
			{
				user->WriteServ("NOTICE " + user->nick + " :Channel " + channel->name + " timedmodes list is full");
				return CMD_FAILURE;
			}
			duration = ServerInstance->Duration(parameters[1]);
			if (duration < 1)
			{
				user->WriteServ("NOTICE " + user->nick + " :Invalid timed mode duration");
				return CMD_FAILURE;
			}
			T.expire = duration + ServerInstance->Time();
			irc::modestacker modes;
			std::vector<std::string> tmpparams = parameters;
			tmpparams.erase(tmpparams.begin()+1); // Remove the duration from the parameters we're giving to Parse()
			Extensible* tmp;
			ServerInstance->Modes->Parse(tmpparams, user, tmp, modes);
			if(modes.sequence.empty()) return CMD_SUCCESS;
			stable_sort(modes.sequence.begin(), modes.sequence.end(), &modeChangeLessThan);
			T.modes = modes;
			ModeHandler* regmh = ServerInstance->Modes->FindMode("registered");
			for (std::vector<irc::modechange>::iterator iter = T.modes.sequence.begin(); iter != T.modes.sequence.end();)
			{
				ModeHandler* mh = ServerInstance->Modes->FindMode(iter->mode);
				if(mh == regmh && !user->HasPrivPermission("channels/set-registration", false)) {
					iter = T.modes.sequence.erase(iter);
					continue;
				}
				if(!iter->adding) {								// if we're about to remove the mode now
					if(mh->GetNumParams(true) && !mh->IsListMode())				// and it needs a parameter to put back
						iter->value = channel->GetModeParameter(mh);			// then save the parameter we have now
					iter->adding = true; 							// and toggle the adding flag
				} else if (!channel->IsModeSet(mh)) {						// but if we're just adding it now
					if(!mh->GetNumParams(false))						// and it doesn't want a parameter to remove
						iter->value = "";						// then make sure it doesn't get one
					iter->adding = false;							// and toggle the adding flag
				} else {									// but if we're changing a mode we already have
					iter->value = channel->GetModeParameter(mh);				// then just save the parameter we have now
				}

				if(mh->GetNumParams(iter->adding) && mh->GetTranslateType() == TR_NICK) {	// if this mode takes a nick as a parameter
					User* u = ServerInstance->FindNick(iter->value);			// look for the user with the given nick
					if(u) iter->value = u->uuid;						// and change it to the UID if we found them
					else {									// otherwise,
						iter = T.modes.sequence.erase(iter);				// drop the mode
						continue;							// and go to the next mode
					}
				}
				++iter;
			}
			for (std::vector<irc::modechange>::iterator iter = modes.sequence.begin(); iter != modes.sequence.end();)
			{
				ModeHandler* mh = ServerInstance->Modes->FindMode(iter->mode);
				if(mh == regmh && !user->HasPrivPermission("channels/set-registration", false)) {
					user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission denied - you may not set the registered channel mode as a timed mode", user->nick.c_str());
					iter = modes.sequence.erase(iter);
					continue;
				}
				if(mh->GetNumParams(iter->adding) && mh->GetTranslateType() == TR_NICK) {	// if this mode takes a nick as a parameter
					User* u = ServerInstance->FindNick(iter->value);			// look for the user with the given nick
					if(u) iter->value = u->uuid;						// and change it to the UID if we found them
														// otherwise, let SendMode remove it and tell the user
				}
				++iter;
			}

			current = &T;
			ServerInstance->SendMode(user, channel, modes, true);
			current = NULL;
			if(T.modes.sequence.empty()) return CMD_SUCCESS;
			if(!existing) {
				existing = new timedmodes();
				tmodes.set(channel, existing);
			}
			else {
				for (timedmodes::iterator i = existing->begin(); i != existing->end(); ++i)
					if(T == *i) {
						user->WriteServ("NOTICE " + user->nick + " :The timed mode given is an exact duplicate of one already set");
						return CMD_FAILURE; // we already have this TimedMode, so this one's a duplicate
					}
			}
			existing->push_back(T);

			modes = toannounce;
			toannounce = T.modes;
			// Tell the channel and other servers what happened
			int maxModeLength = MAXBUF - 24 - channel->name.length() - ConvToStr(T.expire).length();
			while(!toannounce.sequence.empty()) {
				std::vector<std::string> params;
				params.push_back("*");
				params.push_back("TMODE");
				params.push_back(channel->name);
				params.push_back(ConvToStr(T.expire));
				params.push_back(toannounce.popModeLine(FORMAT_NETWORK, maxModeLength, INT_MAX));
				ServerInstance->PI->SendEncapsulatedData(params);
			}
			toannounce = modes;
			std::string msgbegin = "*** " + user->nick + " set the following timed modes lasting for " + ConvToStr(duration) + " seconds: ", modeline;
			maxModeLength = MAXBUF - 15 - ServerInstance->Config->ServerName.length() - channel->name.length() - msgbegin.length();
			while(!toannounce.sequence.empty()) {
				modeline = toannounce.popModeLine(FORMAT_USER, maxModeLength, INT_MAX);
				channel->WriteChannelWithServ(ServerInstance->Config->ServerName.c_str(), "NOTICE %s :%s%s", channel->name.c_str(), msgbegin.c_str(), modeline.c_str());
				ServerInstance->PI->SendChannelNotice(channel, 0, msgbegin + modeline);
			}
			return CMD_SUCCESS;
		}
	}
};

class ModuleTimedModes : public Module
{
	CommandTmode cmd;

 public:
	ModuleTimedModes() : cmd(this) {}

	void init()
	{
		cmd.current = NULL;
		ServerInstance->AddCommand(&cmd);
		ServerInstance->Extensions.Register(&cmd.tmodes);
		Implementation eventlist[] = { I_OnMode, I_OnBackgroundTimer, I_OnSyncChannel };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ~ModuleTimedModes()
	{
	}

	void OnMode(User*, Extensible* dest, const irc::modestacker& modeschanged)
	{
		if(cmd.current)
		{
			std::vector<irc::modechange> newsequence;
			set_intersection(cmd.current->modes.sequence.begin(), cmd.current->modes.sequence.end(),
				modeschanged.sequence.begin(), modeschanged.sequence.end(),
				std::back_inserter(newsequence), &modeChangeLessThan);
			cmd.current->modes.sequence = newsequence;
			cmd.toannounce = modeschanged;
		}
		std::vector<irc::modechange> sortedchanged = modeschanged.sequence;
		stable_sort(sortedchanged.begin(), sortedchanged.end(), &modeChangeLessThan);
		timedmodes* existing = cmd.tmodes.get(dest);
		if(!existing) return;
		for (timedmodes::iterator i = existing->begin(); i != existing->end();)
		{
			std::vector<irc::modechange> newsequence;
			set_difference(i->modes.sequence.begin(), i->modes.sequence.end(),
				sortedchanged.begin(), sortedchanged.end(),
				std::back_inserter(newsequence), &modeChangeLessThan);
			i->modes.sequence = newsequence;
			if(i->modes.sequence.empty()) {
				i = existing->erase(i);
				continue;
			}
			++i;
		}
	}

	virtual void OnBackgroundTimer(time_t curtime)
	{
		for (chan_hash::const_iterator i = ServerInstance->chanlist->begin(); i != ServerInstance->chanlist->end(); ++i)
		{
			timedmodes* existing = cmd.tmodes.get(i->second);
			if(!existing) continue;
			for (timedmodes::iterator j = existing->begin(); j != existing->end();)
			{
				if (curtime > j->expire)
				{
					irc::modestacker modes = j->modes;
					j = existing->erase(j);
					ServerInstance->SendMode(ServerInstance->FakeClient, i->second, modes, true);
					int maxModeLength = MAXBUF - 58 - ServerInstance->Config->ServerName.length() - i->second->name.length();
					std::string message;
					while(!modes.sequence.empty()) {
						message = "*** Timed modes have expired, reverted to: " + modes.popModeLine(FORMAT_USER, maxModeLength, INT_MAX);
						i->second->WriteChannelWithServ(ServerInstance->Config->ServerName.c_str(), "NOTICE %s :%s",
							i->second->name.c_str(), message.c_str());
						ServerInstance->PI->SendChannelNotice(i->second, 0, message);
					}
				}
				else
					++j;
			}
		}
	}

	virtual void OnSyncChannel(Channel* channel, SyncTarget* target)
	{
		timedmodes* existing = cmd.tmodes.get(channel);
		if(!existing) return;
		for (timedmodes::iterator i = existing->begin(); i != existing->end();++i)
		{
			int maxModeLength = MAXBUF - 24 - channel->name.length() - ConvToStr(i->expire).length();
			irc::modestacker modes = i->modes;
			while(!modes.sequence.empty())
				target->SendCommand("ENCAP * TMODE " + channel->name + " " + ConvToStr(i->expire) + " " + modes.popModeLine(FORMAT_NETWORK, maxModeLength, INT_MAX));
		}
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ConfigTagList tags = ServerInstance->Config->GetTags("timedmodes");

		cmd.chanlimits.clear();

		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			// For each <timedmodes> tag
			ConfigTag* c = i->second;
			ListLimit limit;
			limit.mask = c->getString("chan");
			limit.limit = c->getInt("limit");

			if (limit.mask.size() && limit.limit > 0)
				cmd.chanlimits.push_back(limit);
		}
		if (cmd.chanlimits.size() == 0)
		{
			ListLimit limit;
			limit.mask = "*";
			limit.limit = 30;
			cmd.chanlimits.push_back(limit);
		}
	}

	virtual Version GetVersion()
	{
		return Version("Adds timed mode changes", VF_VENDOR);
	}
};

MODULE_INIT(ModuleTimedModes)

