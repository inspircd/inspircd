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
#include "account.h"
#include "nickregister.h"

static dynamic_reference<AccountProvider> accounts("account");

struct NickData
{
	std::string account;
	time_t ts;
	time_t last_used;
	NickData(const std::string& a, time_t t, time_t l)
		: account(a), ts(t), last_used(l) {}
	void update(const NickData& incoming)
	{
		if (ts == incoming.ts && account == incoming.account)
		{
			// match, just update last_used
			if (last_used < incoming.last_used)
				last_used = incoming.last_used;
		}
		else if (ts > incoming.ts || (ts == incoming.ts && account < incoming.account))
		{
			// we got replaced by TS collision
			*this = incoming;
		}
		else
		{
			// we won the TS collision; discard
		}
	}
};

/** Nick mapped to information */
typedef std::map<std::string, NickData> NickMap;
/** Account mapped to nick(s) */
typedef std::multimap<std::string, std::string> OwnerMap;

class RegDB : public NickRegistrationProvider
{
 public:
	NickMap nickinfo;
	OwnerMap nicksowned;
	time_t expiry;

	RegDB(Module* parent) : NickRegistrationProvider(parent) {}

	std::vector<std::string> GetNicks(const std::string& useraccount)
	{
		std::vector<std::string> rv;
		std::pair<OwnerMap::iterator, OwnerMap::iterator> range = nicksowned.equal_range(useraccount);
		for(OwnerMap::iterator i = range.first; i != range.second; i++)
			rv.push_back(i->second);
		return rv;
	}

	std::string GetOwner(const std::string& nick)
	{
		NickMap::iterator it = nickinfo.find(nick);
		if (it == nickinfo.end())
			return "";
		return it->second.account;
	}

	void UpdateLastUse(const std::string& nick)
	{
		NickMap::iterator it = nickinfo.find(nick);
		if (it != nickinfo.end())
			it->second.last_used = ServerInstance->Time();
	}

	void SetOwner(const std::string& nick, const std::string& acct)
	{
		time_t now = ServerInstance->Time();
		SetOwner(nick, acct, now, now);
		parameterlist params;
		params.push_back("*");
		params.push_back("SVSNICKREGISTER");
		params.push_back(nick);
		params.push_back(acct);
		params.push_back(ConvToStr(now));
		params.push_back(ConvToStr(now));
		ServerInstance->PI->SendEncapsulatedData(params);
	}

	void SetOwner(const std::string& nick, const std::string& regaccount, time_t ts, time_t luts)
	{
		NickData value(regaccount, ts, luts);
		NickMap::iterator it = nickinfo.find(nick);
		std::string oldowner = "-";
		if (it == nickinfo.end())
		{
			if (regaccount != "-")
			{
				nickinfo.insert(std::make_pair(nick, value));
				nicksowned.insert(std::make_pair(value.account, nick));
			}
		}
		else
		{
			oldowner = it->second.account;
			it->second.update(value);
			if (it->second.account == oldowner && regaccount != "-")
				return;
			OwnerMap::iterator rev = nicksowned.lower_bound(oldowner);
			while (rev != nicksowned.end() && rev->first == oldowner && rev->second != nick)
				rev++;
			if (rev != nicksowned.end() && rev->first == oldowner)
				nicksowned.erase(rev);
			if (regaccount == "-")
				nickinfo.erase(it);
			else
				nicksowned.insert(std::make_pair(it->second.account, nick));
		}
		NickRegisterChangeEvent(creator, nick, oldowner, regaccount);
	}

	void Clean()
	{
		NickMap::iterator i = nickinfo.begin();
		time_t cutoff = ServerInstance->Time() - expiry;
		while (i != nickinfo.end())
		{
			NickMap::iterator curr = i++;
			if (curr->second.last_used < cutoff)
			{
				OwnerMap::iterator rev = nicksowned.lower_bound(curr->second.account);
				while (rev != nicksowned.end() && rev->first == curr->second.account && rev->second != curr->first)
					rev++;
				if (rev != nicksowned.end() && rev->first == curr->second.account)
					nicksowned.erase(rev);
				nickinfo.erase(curr);
			}
		}
	}
};

class CommandRegisterNick : public Command
{
 public:
	RegDB db;

	int maxreg;
	CommandRegisterNick(Module* parent) : Command(parent, "SVSNICKREGISTER", 4), db(parent)
	{
		syntax = "<nick> <account> <age> <last-used>";
		flags_needed = FLAG_SERVERONLY;
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		std::string nick = parameters[0];
		std::string account = parameters[1];
		time_t ts = atoi(parameters[2].c_str());
		time_t luts = atoi(parameters[3].c_str());

		db.SetOwner(nick, account, ts, luts);
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_OPT_BCAST;
	}
};

class ModuleNickRegister : public Module
{
 public:
	CommandRegisterNick cmd;

	ModuleNickRegister() : cmd(this) {}

	void ReadConfig(ConfigReadStatus& status)
	{
		ConfigTag* tag = status.GetTag("nickregister");
		cmd.db.expiry = ServerInstance->Duration(tag->getString("expiretime", "21d"));
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		ServerInstance->Modules->AddService(cmd.db);
		Implementation eventlist[] = {
			I_OnUserPreNick, I_OnCheckReady, I_OnSyncNetwork, I_OnUserQuit, I_OnGarbageCollect
		};
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnUserPreNick(User* user, const std::string& nick)
	{
		// update timestamp on old nick
		cmd.db.UpdateLastUse(user->nick);

		if (ServerInstance->NICKForced.get(user))
			return MOD_RES_PASSTHRU;

		// check the new nick
		std::string owner = cmd.db.GetOwner(nick);
		std::string acctname = accounts ? accounts->GetAccountName(user) : "";
		if (owner.empty() || owner == acctname)
			return MOD_RES_PASSTHRU;
		if (user->registered == REG_ALL)
		{
			user->WriteNumeric(433, "%s %s :You must be identified to the account '%s' to use this nick",
				user->nick.c_str(), nick.c_str(), owner.c_str());
			return MOD_RES_DENY;
		}
		else
		{
			// allow through to give things like SASL or SQLauth time to return before denying
			user->WriteNumeric(437, "%s %s :This nick requires you to identify to the account '%s'",
				user->nick.c_str(), nick.c_str(), owner.c_str());
			return MOD_RES_PASSTHRU;
		}
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		std::string owner = cmd.db.GetOwner(user->nick);
		if (owner.empty())
			return MOD_RES_PASSTHRU;
		std::string acctname = accounts ? accounts->GetAccountName(user) : "";
		if (owner != acctname)
		{
			user->WriteNumeric(433, "%s %s :Nickname overruled: requires login to the account '%s'",
				user->nick.c_str(), user->nick.c_str(), owner.c_str());
			user->ChangeNick(user->uuid, true);
		}
		return MOD_RES_PASSTHRU;
	}

	void OnUserQuit(User* user, const std::string&, const std::string&)
	{
		cmd.db.UpdateLastUse(user->nick);
	}

	void OnGarbageCollect()
	{
		cmd.db.Clean();
	}

	void OnSyncNetwork(SyncTarget* target)
	{
		for(NickMap::iterator i = cmd.db.nickinfo.begin(); i != cmd.db.nickinfo.end(); i++)
		{
			parameterlist params;
			params.push_back(i->first);
			params.push_back(i->second.account);
			params.push_back(ConvToStr(i->second.ts));
			params.push_back(ConvToStr(i->second.last_used));
			target->SendEncap(cmd.name, params);
		}
	}

	void Prioritize()
	{
		// need to be after all modules that might deny the ready check
		ServerInstance->Modules->SetPriority(this, I_OnCheckReady, PRIORITY_LAST);
	}

	Version GetVersion()
	{
		return Version("Nick registration tracking", VF_VENDOR | VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleNickRegister)
