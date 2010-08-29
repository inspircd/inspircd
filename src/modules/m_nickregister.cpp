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

static dynamic_reference<AccountProvider> account("account");

struct NickData
{
	std::string account;
	time_t ts;
	NickData(const std::string& a, time_t t) : account(a), ts(t) {}
	bool operator<(const NickData& o) const
	{
		if (ts != o.ts)
			return ts < o.ts;
		return account < o.account;
	}
};
typedef std::map<std::string, NickData> OwnerMap;

class CommandRegisterNick : public Command
{
 public:
	OwnerMap nickowner;
	int maxreg;
	CommandRegisterNick(Module* parent) : Command(parent, "NICKREGISTER", 2), maxreg(5)
	{
		syntax = "<nick> <account>";
	}

	inline void setflags(char f) { flags_needed = f; }

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		// TODO convert to numerics for errors
		if (IS_LOCAL(user) && !flags_needed)
		{
			// users can only register their own nick to their own account
			if (irc::string(parameters[0]) != irc::string(user->nick))
			{
				user->WriteServ("NOTICE %s :You can only register your own nick", user->nick.c_str());
				return CMD_FAILURE;
			}
			std::string acctname = account ? account->GetAccountName(user) : "";
			if (parameters[1] != "-" && parameters[1] != acctname)
			{
				user->WriteServ("NOTICE %s :You can only register to your own account", user->nick.c_str());
				return CMD_FAILURE;
			}
			// and they can't register more than 5 per account (or the conf'd max)
			int count = 0;
			for(OwnerMap::iterator i = nickowner.begin(); i != nickowner.end(); i++)
				if (i->second.account == acctname)
					count++;
			if (count > maxreg)
			{
				user->WriteServ("NOTICE %s :You can only %d nicks", user->nick.c_str(), maxreg);
				return CMD_FAILURE;
			}
		}
		if (IS_LOCAL(user) && !ServerInstance->IsNick(parameters[0].c_str(), ServerInstance->Config->Limits.NickMax))
		{
			user->WriteServ("NOTICE %s :Not a valid nick", user->nick.c_str());
			return CMD_FAILURE;
		}

		int ts = ServerInstance->Time();
		if (IS_SERVER(user) && parameters.size() > 2)
		{
			ts = atoi(parameters[2].c_str());
		}
		else
		{
			// TODO allow this to be done by changing API
			std::vector<std::string>& pmod = const_cast<std::vector<std::string>&>(parameters);
			if (pmod.size() > 2)
				pmod[2] = ConvToStr(ts);
			else
				pmod.push_back(ConvToStr(ts));
		}

		NickData value(parameters[1], ts);
		OwnerMap::iterator it = nickowner.find(parameters[0]);
		if (it == nickowner.end())
		{
			if (parameters[1] != "-")
				nickowner.insert(std::make_pair(parameters[0], value));
		}
		else
		{
			if (it->second < value)
				it->second = value;
			if (it->second.account == "-")
				nickowner.erase(it);
		}

		if (IS_LOCAL(user))
			user->WriteServ("NOTICE %s :You have successfully %sregistered the nick %s",
				user->nick.c_str(), parameters[1].empty() ? "un" : "", parameters[0].c_str());
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
		std::string mode = tag->getString("mode", "services");
		if (mode == "services")
			cmd.setflags(FLAG_SERVERONLY);
		else if (mode == "opers")
			cmd.setflags('o');
		else if (mode == "users")
			cmd.setflags(0);
		else
			status.ReportError(tag, "<nickregister:mode> must be one of: services, opers, users");

		cmd.maxreg = tag->getInt("maxperaccount", 5);
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		Implementation eventlist[] = { I_OnUserPreNick, I_OnCheckReady, I_OnSyncNetwork };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}
	
	ModResult OnUserPreNick(User* user, const std::string& nick)
	{
		if (ServerInstance->NICKForced.get(user))
			return MOD_RES_PASSTHRU;
		OwnerMap::iterator it = cmd.nickowner.find(nick);
		if (it == cmd.nickowner.end())
			return MOD_RES_PASSTHRU;
		std::string acctname = account ? account->GetAccountName(user) : "";
		if (it->second.account == acctname)
			return MOD_RES_PASSTHRU;
		if (user->registered == REG_ALL)
		{
			user->WriteNumeric(433, "%s %s :You must be identified to the account '%s' to use this nick",
				user->nick.c_str(), user->nick.c_str(), it->second.account.c_str());
			return MOD_RES_DENY;
		}
		else
		{
			user->WriteNumeric(437, "%s %s :This nick requires you to identify to the account '%s'",
				user->nick.c_str(), user->nick.c_str(), it->second.account.c_str());
			return MOD_RES_PASSTHRU;
		}
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		OwnerMap::iterator it = cmd.nickowner.find(user->nick);
		if (it == cmd.nickowner.end())
			return MOD_RES_PASSTHRU;
		std::string acctname = account ? account->GetAccountName(user) : "";
		if (it->second.account != acctname)
		{
			user->WriteNumeric(433, "%s %s :Nickname overruled: requires login to the account '%s'",
				user->nick.c_str(), user->nick.c_str(), it->second.account.c_str());
			user->ChangeNick(user->uuid, true);
		}
		return MOD_RES_PASSTHRU;
	}
	
	void OnSyncNetwork(SyncTarget* target)
	{
		for(OwnerMap::iterator i = cmd.nickowner.begin(); i != cmd.nickowner.end(); i++)
		{
			parameterlist params;
			params.push_back(i->first);
			params.push_back(i->second.account);
			params.push_back(ConvToStr(i->second.ts));
			target->SendEncap(cmd.name, params);
		}
	}

	Version GetVersion()
	{
		return Version("Nick registration tracking", VF_VENDOR);
	}
};

MODULE_INIT(ModuleNickRegister)
