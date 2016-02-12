/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
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

/* $ModDesc: Provides support for extended-join, away-notify and account-notify CAP capabilities */

#include "inspircd.h"
#include "account.h"
#include "m_cap.h"

class ModuleIRCv3 : public Module
{
	GenericCap cap_accountnotify;
	GenericCap cap_awaynotify;
	GenericCap cap_extendedjoin;
	bool accountnotify;
	bool awaynotify;
	bool extendedjoin;

	CUList last_excepts;

	void WriteNeighboursWithExt(User* user, const std::string& line, const LocalIntExt& ext)
	{
		UserChanList chans(user->chans);

		std::map<User*, bool> exceptions;
		FOREACH_MOD(I_OnBuildNeighborList, OnBuildNeighborList(user, chans, exceptions));

		// Send it to all local users who were explicitly marked as neighbours by modules and have the required ext
		for (std::map<User*, bool>::const_iterator i = exceptions.begin(); i != exceptions.end(); ++i)
		{
			LocalUser* u = IS_LOCAL(i->first);
			if ((u) && (i->second) && (ext.get(u)))
				u->Write(line);
		}

		// Now consider sending it to all other users who has at least a common channel with the user
		std::set<User*> already_sent;
		for (UCListIter i = chans.begin(); i != chans.end(); ++i)
		{
			const UserMembList* userlist = (*i)->GetUsers();
			for (UserMembList::const_iterator m = userlist->begin(); m != userlist->end(); ++m)
			{
				/*
				 * Send the line if the channel member in question meets all of the following criteria:
				 * - local
				 * - not the user who is doing the action (i.e. whose channels we're iterating)
				 * - has the given extension
				 * - not on the except list built by modules
				 * - we haven't sent the line to the member yet
				 *
				 */
				LocalUser* member = IS_LOCAL(m->first);
				if ((member) && (member != user) && (ext.get(member)) && (exceptions.find(member) == exceptions.end()) && (already_sent.insert(member).second))
					member->Write(line);
			}
		}
	}

 public:
	ModuleIRCv3() : cap_accountnotify(this, "account-notify"),
					cap_awaynotify(this, "away-notify"),
					cap_extendedjoin(this, "extended-join")
	{
	}

	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnUserJoin, I_OnPostJoin, I_OnSetAway, I_OnEvent, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnRehash(User* user)
	{
		ConfigTag* conf = ServerInstance->Config->ConfValue("ircv3");
		accountnotify = conf->getBool("accountnotify", conf->getBool("accoutnotify", true));
		awaynotify = conf->getBool("awaynotify", true);
		extendedjoin = conf->getBool("extendedjoin", true);
	}

	void OnEvent(Event& ev)
	{
		if (awaynotify)
			cap_awaynotify.HandleEvent(ev);
		if (extendedjoin)
			cap_extendedjoin.HandleEvent(ev);

		if (accountnotify)
		{
			cap_accountnotify.HandleEvent(ev);

			if (ev.id == "account_login")
			{
				AccountEvent* ae = static_cast<AccountEvent*>(&ev);

				// :nick!user@host ACCOUNT account
				// or
				// :nick!user@host ACCOUNT *
				std::string line =  ":" + ae->user->GetFullHost() + " ACCOUNT ";
				if (ae->account.empty())
					line += "*";
				else
					line += std::string(ae->account);

				WriteNeighboursWithExt(ae->user, line, cap_accountnotify.ext);
			}
		}
	}

	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& excepts)
	{
		// Remember who is not going to see the JOIN because of other modules
		if ((awaynotify) && (IS_AWAY(memb->user)))
			last_excepts = excepts;

		if (!extendedjoin)
			return;

		/*
		 * Send extended joins to clients who have the extended-join capability.
		 * An extended join looks like this:
		 *
		 * :nick!user@host JOIN #chan account :realname
		 *
		 * account is the joining user's account if he's logged in, otherwise it's an asterisk (*).
		 */

		std::string line;
		std::string mode;

		const UserMembList* userlist = memb->chan->GetUsers();
		for (UserMembCIter it = userlist->begin(); it != userlist->end(); ++it)
		{
			// Send the extended join line if the current member is local, has the extended-join cap and isn't excepted
			User* member = IS_LOCAL(it->first);
			if ((member) && (cap_extendedjoin.ext.get(member)) && (excepts.find(member) == excepts.end()))
			{
				// Construct the lines we're going to send if we haven't constructed them already
				if (line.empty())
				{
					bool has_account = false;
					line = ":" + memb->user->GetFullHost() + " JOIN " + memb->chan->name + " ";
					const AccountExtItem* accountext = GetAccountExtItem();
					if (accountext)
					{
						std::string* accountname;
						accountname = accountext->get(memb->user);
						if (accountname)
						{
							line += *accountname;
							has_account = true;
						}
					}

					if (!has_account)
						line += "*";

					line += " :" + memb->user->fullname;

					// If the joining user received privileges from another module then we must send them as well,
					// since silencing the normal join means the MODE will be silenced as well
					if (!memb->modes.empty())
					{
						const std::string& modefrom = ServerInstance->Config->CycleHostsFromUser ? memb->user->GetFullHost() : ServerInstance->Config->ServerName;
						mode = ":" + modefrom + " MODE " + memb->chan->name + " +" + memb->modes;

						for (unsigned int i = 0; i < memb->modes.length(); i++)
							mode += " " + memb->user->nick;
					}
				}

				// Write the JOIN and the MODE, if any
				member->Write(line);
				if ((!mode.empty()) && (member != memb->user))
					member->Write(mode);

				// Prevent the core from sending the JOIN and MODE to this user
				excepts.insert(it->first);
			}
		}
	}

	ModResult OnSetAway(User* user, const std::string &awaymsg)
	{
		if (awaynotify)
		{
			// Going away: n!u@h AWAY :reason
			// Back from away: n!u@h AWAY
			std::string line = ":" + user->GetFullHost() + " AWAY";
			if (!awaymsg.empty())
				line += " :" + awaymsg;

			WriteNeighboursWithExt(user, line, cap_awaynotify.ext);
		}
		return MOD_RES_PASSTHRU;
	}

	void OnPostJoin(Membership *memb)
	{
		if ((!awaynotify) || (!IS_AWAY(memb->user)))
			return;

		std::string line = ":" + memb->user->GetFullHost() + " AWAY :" + memb->user->awaymsg;

		const UserMembList* userlist = memb->chan->GetUsers();
		for (UserMembCIter it = userlist->begin(); it != userlist->end(); ++it)
		{
			// Send the away notify line if the current member is local, has the away-notify cap and isn't excepted
			User* member = IS_LOCAL(it->first);
			if ((member) && (cap_awaynotify.ext.get(member)) && (last_excepts.find(member) == last_excepts.end()) && (it->second != memb))
			{
				member->Write(line);
			}
		}

		last_excepts.clear();
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnUserJoin, PRIORITY_LAST);
	}

	Version GetVersion()
	{
		return Version("Provides support for extended-join, away-notify and account-notify CAP capabilities", VF_VENDOR);
	}
};

MODULE_INIT(ModuleIRCv3)
