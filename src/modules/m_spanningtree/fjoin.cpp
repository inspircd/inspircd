/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


/** FJOIN, almost identical to TS6 SJOIN, except for nicklist handling. */
bool TreeSocket::ForceJoin(const std::string &source, parameterlist &params)
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
	 */
	if (params.size() < 3)
		return true;

	irc::modestacker modestack(ServerInstance, true);			/* Modes to apply from the users in the user list */
	User* who = NULL;		   				/* User we are currently checking */
	std::string channel = params[0];				/* Channel name, as a string */
	time_t TS = atoi(params[1].c_str());    			/* Timestamp given to us for remote side */
	irc::tokenstream users((params.size() > 3) ? params[params.size() - 1] : "");   /* users from the user list */
	bool apply_other_sides_modes = true;				/* True if we are accepting the other side's modes */
	Channel* chan = this->ServerInstance->FindChan(channel);		/* The channel we're sending joins to */
	bool created = !chan;						/* True if the channel doesnt exist here yet */
	std::string item;						/* One item in the list of nicks */

	if (params.size() > 3)
		params[params.size() - 1] = ":" + params[params.size() - 1];

	Utils->DoOneToAllButSender(source,"FJOIN",params,source);

	if (!TS)
	{
		ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"*** BUG? *** TS of 0 sent to FJOIN. Are some services authors smoking craq, or is it 1970 again?. Dropped.");
		ServerInstance->SNO->WriteToSnoMask('d', "WARNING: The server %s is sending FJOIN with a TS of zero. Total craq. Command was dropped.", source.c_str());
		return true;
	}

	if (created)
	{
		chan = new Channel(ServerInstance, channel, TS);
		ServerInstance->SNO->WriteToSnoMask('d', "Creation FJOIN recieved for %s, timestamp: %lu", chan->name.c_str(), (unsigned long)TS);
	}
	else
	{
		time_t ourTS = chan->age;

		if (TS != ourTS)
			ServerInstance->SNO->WriteToSnoMask('d', "Merge FJOIN recieved for %s, ourTS: %lu, TS: %lu, difference: %lu",
				chan->name.c_str(), (unsigned long)ourTS, (unsigned long)TS, (unsigned long)(ourTS - TS));
		/* If our TS is less than theirs, we dont accept their modes */
		if (ourTS < TS)
		{
			ServerInstance->SNO->WriteToSnoMask('d', "NOT Applying modes from other side");
			apply_other_sides_modes = false;
		}
		else if (ourTS > TS)
		{
			/* Our TS greater than theirs, clear all our modes from the channel, accept theirs. */
			ServerInstance->SNO->WriteToSnoMask('d', "Removing our modes, accepting remote");
			parameterlist param_list;
			if (Utils->AnnounceTSChange && chan)
				chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :TS for %s changed from %lu to %lu", chan->name.c_str(), chan->name.c_str(), (unsigned long) ourTS, (unsigned long) TS);
			ourTS = TS;
			chan->age = TS;
			param_list.push_back(channel);
			this->RemoveStatus(ServerInstance->Config->GetSID(), param_list);
		}
		// The silent case here is ourTS == TS, we don't need to remove modes here, just to merge them later on.
	}

	/* First up, apply their modes if they won the TS war */
	if (apply_other_sides_modes)
	{
		unsigned int idx = 2;
		std::vector<std::string> modelist;

		// Mode parser needs to know what channel to act on.
		modelist.push_back(params[0]);

		/* Remember, params[params.size() - 1] is nicklist, and we don't want to apply *that* */
		for (idx = 2; idx != (params.size() - 1); idx++)
		{
			modelist.push_back(params[idx]);
		}

		this->ServerInstance->SendMode(modelist, Utils->ServerUser);
	}

	/* Now, process every 'modes,nick' pair */
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
				ModeHandler *mh = ServerInstance->Modes->FindMode(*unparsedmodes, MODETYPE_CHANNEL);
				if (mh)
					modes += *unparsedmodes;
				else
				{
					this->SendError(std::string("Unknown status mode '")+(*unparsedmodes)+"' in FJOIN");
					return false;
				}

				usr++;
				unparsedmodes++;
			}

			/* Advance past the comma, to the nick */
			usr++;

			/* Check the user actually exists */
			who = this->ServerInstance->FindUUID(usr);
			if (who)
			{
				/* Check that the user's 'direction' is correct */
				TreeServer* route_back_again = Utils->BestRouteTo(who->server);
				if ((!route_back_again) || (route_back_again->GetSocket() != this))
					continue;

				/* Add any modes this user had to the mode stack */
				for (std::string::iterator x = modes.begin(); x != modes.end(); ++x)
					modestack.Push(*x, who->nick);

				Channel::JoinUser(this->ServerInstance, who, channel.c_str(), true, "", route_back_again->bursting, TS);
			}
			else
			{
				ServerInstance->Logs->Log("m_spanningtree",SPARSE, "Ignored nonexistant user %s in fjoin to %s (probably quit?)", usr, channel.c_str());
				continue;
			}
		}
	}

	/* Flush mode stacker if we lost the FJOIN or had equal TS */
	if (apply_other_sides_modes)
	{
		parameterlist stackresult;
		stackresult.push_back(channel);

		while (modestack.GetStackedLine(stackresult))
		{
			ServerInstance->SendMode(stackresult, Utils->ServerUser);
			stackresult.erase(stackresult.begin() + 1, stackresult.end());
		}
	}

	return true;
}

bool TreeSocket::RemoveStatus(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 1)
		return true;

	Channel* c = ServerInstance->FindChan(params[0]);

	if (c)
	{
		irc::modestacker stack(ServerInstance, false);
		parameterlist stackresult;
		stackresult.push_back(c->name);

		for (char modeletter = 'A'; modeletter <= 'z'; ++modeletter)
		{
			ModeHandler* mh = ServerInstance->Modes->FindMode(modeletter, MODETYPE_CHANNEL);

			/* Passing a pointer to a modestacker here causes the mode to be put onto the mode stack,
			 * rather than applied immediately. Module unloads require this to be done immediately,
			 * for this function we require tidyness instead. Fixes bug #493
			 */
			if (mh)
				mh->RemoveMode(c, &stack);
		}

		while (stack.GetStackedLine(stackresult))
		{
			ServerInstance->SendMode(stackresult, Utils->ServerUser);
			stackresult.erase(stackresult.begin() + 1, stackresult.end());
		}
	}
	return true;
}

