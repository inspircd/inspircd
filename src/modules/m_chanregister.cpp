/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
 *   Copyright (C) 2010 webczat <webczat_200@poczta.onet.pl>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "account.h"

/* $ModDesc: Provides channel mode +r for channel registration */

static dynamic_reference<AccountDBProvider> db("accountdb");

/* Some function for checking if the selected account is a registrant for a selected channel, for writing less code later */
static bool IsRegistrant (ModeHandler *mh, Channel *chan, const irc::string &account)
{
	irc::string token;
	irc::commasepstream registrantnames(chan->GetModeParameter (mh));
	while (registrantnames.GetToken (token))
	{
		if (token == account)
			return true;
	}
	return false;
}

/* The last_activity extension item: last part from the channel, used to implement expiry */
class ChanExpiryExtItem : public ExtensionItem
{
	public:
	ChanExpiryExtItem (Module *owner) : ExtensionItem (EXTENSIBLE_CHANNEL, "last_activity", owner)
	{

	}
	/* a function to get pointer to the timestamp for a given channel */
	time_t get (const Extensible *container) const
	{
		return reinterpret_cast<time_t> (get_raw (container));
	}
	void set (Extensible *container, time_t value)
	{
		set_raw (container, reinterpret_cast<void *> (value));
	}
	void unset (Extensible *container)
	{
		unset_raw (container);
	}
	void free (void *value)
	{
	}
	/* task of this function is to make the given string into the time_t and to put it in the extension item for the given container */
	void unserialize (SerializeFormat format, Extensible *container, const std::string &value)
	{
		/* independently of the format, the string is always the timestamp */
		/* if nothing happens, we are sure that no one will normally send the metadata except netburst */
		/* if this will be a netburst, we are sure no one will unset it, it just won't be send at all */
		/* so string can't be empty, and thus, the extension item's value can be not found, but can't be 0 */
		/* get the null terminated string from the std::string class, convert it to the integer/time_t */
		time_t newtime = atol (value.c_str ( ));
		/* if newtime is less or equal than the current time set then don't change */
		if (newtime <= get (container))
			return;
		/* if not */
		set (container, newtime);
	}
	std::string serialize (SerializeFormat format, const Extensible *container, void *item) const
	{
		/* we got a value, if it is null then return empty string to deny sending metadata */
		/* if it is not null, it's a pointer to time_t, so cast it, convert to string and return */
		if (!item) return "";
		time_t v = reinterpret_cast<time_t> (item);
		return ConvToStr (v);
	}
};

/* class for handling +r mode */
class RegisterModeHandler : public ParamChannelModeHandler
{
 public:
	ChanExpiryExtItem last_activity;
	dynamic_reference<AccountProvider> account;
	bool verbose;
	int chanlimit;
	RegisterModeHandler(Module *me) : ParamChannelModeHandler(me, "registered", 'r'),
		last_activity(me), account("account"), verbose(true), chanlimit(10)
	{
		fixed_letter = false;
	}

	void OnParameterMissing (User *user, User*, Channel *chan, std::string &param)
	{
		// put the user's own account name in the parameter, if missing
		if (account)
			param = account->GetAccountName(user);
	}
	/* make access checks */
	void AccessCheck(ModePermissionData& perm)
	{
		irc::string acctname = account ? account->GetAccountName(perm.source) : "";

		if (perm.mc.adding)
		{
			if (perm.chan->IsModeSet(this))
			{
				/* changing registration: must be a registrant */
				if (acctname.empty())
				{
					// not logged in: different error message
					perm.ErrorNumeric(ERR_CHANOPRIVSNEEDED, "%s :You must be logged in to an account to reregister a channel",
						perm.chan->name.c_str());
					perm.result = MOD_RES_DENY;
					return;
				}
				if (!IsRegistrant (this, perm.chan, acctname))
				{
					// no matching account name
					perm.ErrorNumeric(ERR_CHANOPRIVSNEEDED, "%s :Only a registrant of a channel may reregister it",
						perm.chan->name.c_str());
					perm.result = MOD_RES_DENY;
					return;
				}
			}
			if (perm.source->HasPrivPermission("channels/set-registration", false))
				return;
			/* new functionality, you can't register any channel if you don't have the oper privilege and chanlimit == 0 */
			if (chanlimit == 0)
			{
				perm.ErrorNumeric (ERR_NOPRIVILEGES, ":Permission denied - only IRC operators may register channels");
				perm.result = MOD_RES_DENY;
				return;
			}
			/* calculate how many channels the user has registered himself */
			int chans = 0;
			for (chan_hash::const_iterator it = ServerInstance->chanlist->begin ( ); it != ServerInstance->chanlist->end ( ); it++)
			{
				if (it->second != perm.chan && it->second->IsModeSet (this))
				{
					irc::commasepstream registrantnames (it->second->GetModeParameter (this));
					irc::string token;
					registrantnames.GetToken (token);
					if (token == acctname) chans++;
				}
			}
			/* check for limit */
			if (chans >= chanlimit)
			{
				perm.ErrorNumeric(ERR_CHANOPRIVSNEEDED, "%s :You can't register more than %i channels under one account", perm.chan->name.c_str(), chans);
				perm.result = MOD_RES_DENY;
				return;
			}
			/* otherwise, you can only set it to your own account name */
			irc::commasepstream registrantnames(perm.mc.value);
			irc::string registrantname;
			/* if the account name was not given, is empty or is not equal to the given parameter, deny */
			if (acctname.empty() || !registrantnames.GetToken(registrantname) || acctname != registrantname)
			{
				perm.ErrorNumeric(ERR_CHANOPRIVSNEEDED, "%s :The first account in the %sregistrant list must be your own",
					perm.chan->name.c_str(), perm.chan->IsModeSet(this) ? "new " : "");
				perm.result = MOD_RES_DENY;
			}
		}
		else
		{
			/* removing a mode: must be a registrant */
			if (acctname.empty())
			{
				// not logged in: different error message
				perm.ErrorNumeric(ERR_CHANOPRIVSNEEDED, "%s :You must be logged in to an account to unregister a channel",
					perm.chan->name.c_str());
				perm.result = MOD_RES_DENY;
				return;
			}
			if (IsRegistrant (this, perm.chan, acctname))
				return;
			// no matching account name
			perm.ErrorNumeric(ERR_CHANOPRIVSNEEDED, "%s :Only a registrant of a channel may unregister it",
				perm.chan->name.c_str());
			perm.result = MOD_RES_DENY;
		}
	}

	ModeAction OnModeChange (User *source, User*, Channel *chan, std::string &param, bool adding)
	{
		if (adding)
		{
			std::string now = chan->GetModeParameter(this), token;
			if(!IS_SERVER(source) && db)
			{
				irc::commasepstream registrantnames(param);
				std::vector<irc::string> registrants;
				while (registrantnames.GetToken (token))
					if (db->GetAccount(token, false))
						registrants.push_back(token);
				if(registrants.empty())
					return MODEACTION_DENY;
				param.clear();
				for(std::vector<irc::string>::iterator i = registrants.begin(); i != registrants.end(); ++i)
					param.append(*i).push_back(',');
				param = param.substr(0, param.size() - 1);
			}
			if (param == now)
				return MODEACTION_DENY;
			if (!now.empty())
			{
				ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(source) || source == ServerInstance->FakeClient ? 'r' : 'R', "%s changed registration of %s from %s to %s",
					source->GetFullHost().c_str(), chan->name.c_str(), now.c_str(), param.c_str());
				if (verbose)
					chan->WriteChannel(ServerInstance->FakeClient, "NOTICE %s :This channel has been reregistered (%s -> %s)",
						chan->name.c_str(), now.c_str(), param.c_str());
			}
			/* servers are expected to notice their reasons themselves */
			else if (!IS_SERVER(source))
			{
				/* first, send a message to all ircops with snomask +r set */
				ServerInstance->SNO->WriteToSnoMask (IS_LOCAL (source) ? 'r' : 'R', "%s registered channel %s to account %s", source->GetFullHost ( ).c_str ( ), chan->name.c_str ( ), param.c_str ( ));
				/* now, send similar to channel */
				if (verbose)
					chan->WriteChannel (ServerInstance->FakeClient, "NOTICE %s :This channel has been registered",
					chan->name.c_str ( ));
			}
			last_activity.set(chan, ServerInstance->Time());
			chan->SetModeParam (this, param);
		}
		else
		{
			/* servers are expected to notice their reasons themselves */
			if (!IS_SERVER(source))
			{
				/* it is set, so first send a server notice to all ircops using +r snomask that this channel has been unregistered */
				ServerInstance->SNO->WriteToSnoMask (IS_LOCAL (source) ? 'r' : 'R', "channel %s unregistered by %s", chan->name.c_str ( ), source->GetFullHost ( ).c_str ( ));
				/* then send this important message to the channel as the notice */
				if (verbose)
					chan->WriteChannel (ServerInstance->FakeClient, "NOTICE %s :This channel has been unregistered",
					chan->name.c_str ( ));
			}
			chan->SetModeParam(this, "");
			last_activity.unset(chan);
			/* now, if usercount is 0, delete the channel */
			if (!chan->GetUserCounter())
				chan->DelUser(ServerInstance->FakeClient);
		}
		return MODEACTION_ALLOW;
	}

	void set_prefixrequired (ModeHandler *mh)
	{
		levelrequired = mh->GetPrefixRank();
	}
};

class ChannelRegistrationModule : public Module
{
 private:
	RegisterModeHandler mh;

	time_t expiretime;

	/* check if the channel given as a parameter expired */
	bool Expired (Channel *chan)
	{
		// can't expire if +r isn't set
		if (!chan->IsModeSet(&mh))
			return false;
		// won't expire if +P is set
		if (chan->IsModeSet("permanent"))
			return false;
		// is it too old?
		if (ServerInstance->Time() - expiretime >= mh.last_activity.get(chan))
			return true;
		return false;
	}

 public:
	/* module constructor, for initializing stuff */
	ChannelRegistrationModule() : mh(this)
	{
	}
	/* get module version and flags. */
	Version GetVersion()
	{
		return Version("Provides channel mode +r for channel registration", VF_VENDOR);
	}
	void init ( )
	{
		ServerInstance->SNO->EnableSnomask ('r', "CHANREGISTER");
		Implementation eventlist[] = {
			I_OnCheckJoin, I_OnPermissionCheck, I_OnChannelPreDelete,
			I_OnUserQuit, I_OnUserPart, I_OnUserKick, I_OnGarbageCollect
		};

		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		ServerInstance->Modules->AddService(mh);
		ServerInstance->Modules->AddService(mh.last_activity);
	}
	/* rehash event */
	void ReadConfig(ConfigReadStatus& status)
	{
		ConfigTag *tag = ServerInstance->Config->GetTag ("chanregister");

		/* expiration is in user-readable time format */
		expiretime = ServerInstance->Duration (tag->getString ("expiretime", "21d"));
		mh.verbose = tag->getBool ("verbose", true);
		mh.chanlimit = tag->getInt ("limit", 10);

		std::string prefixmode = tag->getString("prefix", "op");
		ModeHandler *prefixmodehandler = ServerInstance->Modes->FindMode(prefixmode);
		if (prefixmodehandler)
			mh.set_prefixrequired (prefixmodehandler);
		else
			status.ReportError(tag, "The given prefix level was not found");
	}
	/* OnCheckJoin - this is an event for checking permissions of some user to join some channel, it is used to allow joining by registrants even when
banned */
	void OnCheckJoin (ChannelPermissionData &joindata)
	{
		/* if channel is null, it's not something that is done on channels, or it is but channel is being created, so no registrant in it */
		if (!joindata.chan)
			return;
		/* return if mode is not set and channel is unregistered */
		if (!joindata.chan->IsModeSet (&mh))
			return;
		/* get user's account name */
		irc::string acctname = mh.account ? mh.account->GetAccountName(joindata.source) : "";
		/* if account is not found or empty, we can be really sure that we really aren't registrant of any channel */
		if (acctname.empty())
			return;
		/* it is set and we have parameter, get it */
		if (IsRegistrant (&mh, joindata.chan, acctname))
		{
			/* if registrantname and account name are the same, override */
			joindata.result = MOD_RES_ALLOW;
			return;
		}
	}
	/* check permissions */
	void OnPermissionCheck (PermissionData &perm)
	{
		/* if channel is null, proceed normally */
		if (!perm.chan)
			return;
		/* if not, but name of permission is join, proceed normally because it was checked */
		if (perm.name == "join")
			return;
		/* if permission is implicit, proceed normally, as registrants may not want these permissions */
		if (perm.implicit)
			return;
		/* if +r mode is being changed, return: we keep the normal checks here */
		if (perm.name == "mode/registered")
			return;
		/* if anything is ok, but mode +r is unset, return and check perms normally */
		if (!perm.chan->IsModeSet (&mh))
			return;
		/* if set, get the registrant account name */
		/* get account name of the current user */
		irc::string acctname = mh.account ? mh.account->GetAccountName(perm.source) : "";
		/* if user is not logged in then return */
		if (acctname.empty())
			return;
		if (IsRegistrant (&mh, perm.chan, acctname))
		{
			/* if ok, then set result to allow if registrant name matches account name */
			perm.result = MOD_RES_ALLOW;
			return;
		}
	}
	/* called before channel is being deleted */
	ModResult OnChannelPreDelete (Channel *chan)
	{
		/* return 1 to prevent channel deletion if channel is registered */
		if (chan->IsModeSet(&mh))
			return MOD_RES_DENY;
		/* in other case, return 0, to allow channel deletion */
		return MOD_RES_PASSTHRU;
	}
	/* when someone unsets +r, OnMode event won't mark the database as requiring saving, but it requires to be saved, and because of this, this event
	handler is needed */
	/* this is called when some user parts a channel and is used to record the time user parted as the last activity time */
	/* this is done only if channel is registered, and is used only when it's empty */
	void OnUserPart (Membership *memb, std::string &msg, CUList &except_list)
	{
		/* we have some membership, let's set this last part time now */
		if (memb->chan->IsModeSet (&mh))
			mh.last_activity.set (memb->chan, ServerInstance->Time ( ));
	}
	/* called when the user is being kicked, it's also used for setting last activity time */
	void OnUserKick (User *source, Membership *memb, const std::string &reason, CUList &except_list)
	{
		/* the same */
		if (memb->chan->IsModeSet (&mh))
			mh.last_activity.set (memb->chan, ServerInstance->Time ( ));
	}
	/* if user quits, set last activity time for each channel he was in that are registered */
	void OnUserQuit (User *user, const std::string &msg, const std::string &opermsg)
	{
		/* iterate through a channel list */
		for (UCListIter it = user->chans.begin ( ); it != user->chans.end ( ); it++)
		{
			/* for each membership, check if channel is registered, if it is, set last activity time for it */
			if (it->chan->IsModeSet (&mh))
				mh.last_activity.set (it->chan, ServerInstance->Time ( ));
		}
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnGarbageCollect, PRIORITY_AFTER, ServerInstance->Modules->Find("m_account_register.so"));
	}

	/* called once a hour to expire expired channels */
	void OnGarbageCollect ( )
	{
		chan_hash::const_iterator it = ServerInstance->chanlist->begin();
		std::vector<irc::string> registrants;
		bool listChanged;
		while (it != ServerInstance->chanlist->end())
		{
			Channel* c = it++->second;
			/* check if the channel was expired and is empty at the same time */
			if (c->GetUserCounter() == 0 && Expired(c))
			{
				/* send a notice */
				ServerInstance->SNO->WriteGlobalSno('r', "Channel %s was expired", c->name.c_str());
				/* send mode -r */
				irc::modestacker ms;
				ms.push(irc::modechange(mh.id, "", false));
				ServerInstance->SendMode(ServerInstance->FakeClient, c, ms, true);
			}

			if(!c->IsModeSet(&mh))
				continue;
			if(!db)			// if we're using a services packages for nick registration rather than the account system,
				continue;	// then we can't check whether registrants exist anymore, so we just go to the next channel
			irc::string token;
			irc::commasepstream registrantnames(c->GetModeParameter (&mh));
			registrants.clear();
			listChanged = false;
			registrantnames.GetToken(token);
			if(db->GetAccount(token, false))
				registrants.push_back(token);
			else
			{
				/* send a notice */
				ServerInstance->SNO->WriteGlobalSno('r', "Channel %s was deregistered because the primary registrant account was missing", c->name.c_str());
				/* send mode -r */
				irc::modestacker ms;
				ms.push(irc::modechange(mh.id, "", false));
				ServerInstance->SendMode(ServerInstance->FakeClient, c, ms, true);
				continue;
			}
			while (registrantnames.GetToken (token))
			{
				if (db->GetAccount(token, false))
					registrants.push_back(token);
				else
					listChanged = true;
			}
			if(listChanged)
			{
				std::string newvalue;
				for(std::vector<irc::string>::iterator i = registrants.begin(); i != registrants.end(); ++i)
					newvalue.append(*i).push_back(',');
				irc::modestacker ms;
				ms.push(irc::modechange(mh.id, newvalue.substr(0, newvalue.length() - 1), true));
				ServerInstance->SendMode(ServerInstance->FakeClient, c, ms, true);
			}
		}
	}
};
/* register the module */
MODULE_INIT(ChannelRegistrationModule)
