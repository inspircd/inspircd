/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"

#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


/** FJOIN, similar to TS6 SJOIN, but not quite. */
bool TreeSocket::ForceJoin(const std::string &source, std::deque<std::string> &params)
{
	/* 1.1 FJOIN works as follows:
	 *
	 * Each FJOIN is sent along with a timestamp, and the side with the lowest
	 * timestamp 'wins'. From this point on we will refer to this side as the
	 * winner. The side with the higher timestamp loses, from this point on we
	 * will call this side the loser or losing side. This should be familiar to
	 * anyone who's dealt with dreamforge or TS6 before.
	 *
	 * When two sides of a split heal and this occurs, the following things
	 * will happen:
	 *
	 * If the timestamps are exactly equal, both sides merge their privilages
	 * and users, as in InspIRCd 1.0 and ircd2.8. The channels have not been
	 * re-created during a split, this is safe to do.
	 *
	 * If the timestamps are NOT equal, the losing side removes all of its
	 * modes from the channel, before introducing new users into the channel
	 * which are listed in the FJOIN command's parameters. The losing side then
	 * LOWERS its timestamp value of the channel to match that of the winning
	 * side, and the modes of the users of the winning side are merged in with
	 * the losing side.
	 *
	 * The winning side on the other hand will ignore all user modes from the
	 * losing side, so only its own modes get applied. Life is simple for those
	 * who succeed at internets. :-)
	 *
	 * NOTE: Unlike TS6 and dreamforge and other protocols which have SJOIN,
	 * FJOIN does not contain the simple-modes such as +iklmnsp. Why not,
	 * you ask? Well, quite simply because we don't need to. They'll be sent
	 * after the FJOIN by FMODE, and FMODE is timestamped, so in the event
	 * the losing side sends any modes for the channel which shouldnt win,
	 * they wont as their timestamp will be too high :-)
	 */

	if (params.size() < 2)
		return true;

	irc::modestacker modestack(true);				/* Modes to apply from the users in the user list */
	User* who = NULL;		   				/* User we are currently checking */
	std::string channel = params[0];				/* Channel name, as a string */
	time_t TS = atoi(params[1].c_str());    			/* Timestamp given to us for remote side */
	irc::tokenstream users((params.size() > 2) ? params[2] : "");   /* users from the user list */
	bool apply_other_sides_modes = true;				/* True if we are accepting the other side's modes */
	Channel* chan = this->Instance->FindChan(channel);		/* The channel we're sending joins to */
	time_t ourTS = chan ? chan->age : Instance->Time()+600;	/* The TS of our side of the link */
	bool created = !chan;						/* True if the channel doesnt exist here yet */
	std::string item;						/* One item in the list of nicks */

	if (params.size() > 2)
		params[2] = ":" + params[2];
		
	Utils->DoOneToAllButSender(source,"FJOIN",params,source);

	if (!TS)
	{
		Instance->Logs->Log("m_spanningtree",DEFAULT,"*** BUG? *** TS of 0 sent to FJOIN. Are some services authors smoking craq, or is it 1970 again?. Dropped.");
		Instance->SNO->WriteToSnoMask('d', "WARNING: The server %s is sending FJOIN with a TS of zero. Total craq. Command was dropped.", source.c_str());
		return true;
	}

	if (created)
		chan = new Channel(Instance, channel, ourTS);

	/* If our TS is less than theirs, we dont accept their modes */
	if (ourTS < TS)
		apply_other_sides_modes = false;

	/* Our TS greater than theirs, clear all our modes from the channel, accept theirs. */
	if (ourTS > TS)
	{
		std::deque<std::string> param_list;
		if (Utils->AnnounceTSChange && chan)
			chan->WriteChannelWithServ(Instance->Config->ServerName, "NOTICE %s :TS for %s changed from %lu to %lu", chan->name, chan->name, (unsigned long) ourTS, (unsigned long) TS);
		ourTS = TS;
		if (!created)
		{
			chan->age = TS;
			param_list.push_back(channel);
			this->RemoveStatus(Instance->Config->GetSID(), param_list);
		}
	}

	/* Now, process every 'prefixes,nick' pair */
	while (users.GetToken(item))
	{
		const char* usr = item.c_str();
		if (usr && *usr)
		{
			const char* unparsedmodes = usr;
			std::string modes;


			/* Iterate through all modes for this user and check they are valid. */
			while ((*unparsedmodes) && (*unparsedmodes != ','))
			{
				ModeHandler *mh = Instance->Modes->FindMode(*unparsedmodes, MODETYPE_CHANNEL);
				if (mh)
					modes += *unparsedmodes;
				else
				{
					this->SendError(std::string("Invalid prefix '")+(*unparsedmodes)+"' in FJOIN");
					return false;
				}

				usr++;
				unparsedmodes++;
			}

			/* Advance past the comma, to the nick */
			usr++;
			
			/* Check the user actually exists */
			who = this->Instance->FindUUID(usr);
			if (who)
			{
				/* Check that the user's 'direction' is correct */
				TreeServer* route_back_again = Utils->BestRouteTo(who->server);
				if ((!route_back_again) || (route_back_again->GetSocket() != this))
					continue;

				/* Add any modes this user had to the mode stack */
				for (std::string::iterator x = modes.begin(); x != modes.end(); ++x)
					modestack.Push(*x, who->nick);

				Channel::JoinUser(this->Instance, who, channel.c_str(), true, "", true, TS);
			}
			else
			{
				Instance->Logs->Log("m_spanningtree",SPARSE,"Warning! Invalid user %s in FJOIN to channel %s IGNORED", usr, channel.c_str());
				continue;
			}
		}
	}

	/* Flush mode stacker if we lost the FJOIN or had equal TS */
	if (apply_other_sides_modes)
	{
		std::deque<std::string> stackresult;
		const char* mode_junk[MAXMODES+2];
		mode_junk[0] = channel.c_str();

		while (modestack.GetStackedLine(stackresult))
		{
			for (size_t j = 0; j < stackresult.size(); j++)
			{
				mode_junk[j+1] = stackresult[j].c_str();
			}
			Instance->SendMode(mode_junk, stackresult.size() + 1, Instance->FakeClient);
		}
	}

	return true;
}

/** TODO: This creates a total mess of output and needs to really use irc::modestacker.
 */
bool TreeSocket::RemoveStatus(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return true;

	Channel* c = Instance->FindChan(params[0]);

	if (c)
	{
		for (char modeletter = 'A'; modeletter <= 'z'; modeletter++)
		{
			ModeHandler* mh = Instance->Modes->FindMode(modeletter, MODETYPE_CHANNEL);
			if (mh)
				mh->RemoveMode(c);
		}
	}
	return true;
}
 
