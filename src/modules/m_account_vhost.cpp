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

/* $ModDesc: Allow vhosts to be set on accounts */

static dynamic_reference<AccountProvider> account("account");
static dynamic_reference<AccountDBProvider> db("accountdb");

/** Handle /ACCTVHOST
 */
class CommandAcctvhost : public Command
{
	char* hostmap;

	void UpdateVhosts(irc::string acctname, std::string new_vhost)
	{
		for (user_hash::const_iterator i = ServerInstance->Users->clientlist->begin(); i != ServerInstance->Users->clientlist->end(); ++i)
		{
			if(!IS_LOCAL(i->second) || acctname != account->GetAccountName(i->second))
				continue;
			i->second->ChangeDisplayedHost(new_vhost.c_str());
		}
	}

 public:
	TSStringExtItem vhost;
	CommandAcctvhost(Module* Creator, char* hmap) : Command(Creator,"ACCTVHOST", 1, 2), hostmap(hmap), vhost("vhost", Creator)
	{
		flags_needed = 'o'; syntax = "<account name> [vhost]";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		AccountDBEntry* entry = db->GetAccount(parameters[0], false);
		if(!entry)
		{
			user->WriteServ("NOTICE " + user->nick + " :No such account");
			return CMD_FAILURE;
		}
		if(parameters.size() > 1 && !parameters[1].empty())
		{
			if(IS_LOCAL(user))
			{
				size_t len = 0;
				for (std::string::const_iterator x = parameters[1].begin(); x != parameters[1].end(); ++x, ++len)
				{
					if (!hostmap[(const unsigned char)*x])
					{
						user->WriteServ("NOTICE " + user->nick + " :Invalid characters in hostname");
						return CMD_FAILURE;
					}
				}
				if (len > 64)
				{
					user->WriteServ("NOTICE " + user->nick + " :Host too long");
					return CMD_FAILURE;
				}
				vhost.set(entry, std::make_pair(ServerInstance->Time(), parameters[1]));
				db->SendUpdate(entry, "vhost");
				ServerInstance->SNO->WriteGlobalSno('a', "%s used ACCTVHOST to set the vhost of account %s to %s", user->nick.c_str(), entry->name.c_str(), parameters[1].c_str());
				user->WriteServ("NOTICE " + user->nick + " :Account " + std::string(entry->name) + " vhost set to " + parameters[1]);
			}
			UpdateVhosts(entry->name, parameters[1]);
		}
		else if(IS_LOCAL(user))
		{
			vhost.set(entry, std::make_pair(ServerInstance->Time(), ""));
			db->SendUpdate(entry, "vhost");
			ServerInstance->SNO->WriteGlobalSno('a', "%s used ACCTVHOST to remove the vhost of account %s", user->nick.c_str(), entry->name.c_str());
			user->WriteServ("NOTICE " + user->nick + " :Account " + std::string(entry->name) + " vhost removed");
		}
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_OPT_BCAST;
	}
};

class ModuleAccountVhost : public Module
{
	char hostmap[256];
	CommandAcctvhost cmd_acctvhost;

 public:
	ModuleAccountVhost() : cmd_acctvhost(this, hostmap)
	{
	}

	void init()
	{
		if(!db) throw ModuleException("m_account_vhost requires that m_account be loaded");
		ServerInstance->Modules->AddService(cmd_acctvhost);
		ServerInstance->Modules->AddService(cmd_acctvhost.vhost);
		Implementation eventlist[] = { I_OnEvent, I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void ReadConfig(ConfigReadStatus&)
	{
		std::string hmap = ServerInstance->Config->GetTag("hostname")->getString("charmap");

		if (hmap.empty())
			hmap = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789";

		memset(hostmap, 0, sizeof(hostmap));
		for (std::string::iterator n = hmap.begin(); n != hmap.end(); n++)
			hostmap[(unsigned char)*n] = 1;
	}

	void OnEvent(Event& event)
	{
		if(event.id == "account_login"){
			AccountEvent& acct_event = static_cast<AccountEvent&>(event);
			if(!IS_LOCAL(acct_event.user) || acct_event.user->registered != REG_ALL)
				return;
			AccountDBEntry* entry = db->GetAccount(acct_event.account, false);
			if(!entry)
				return;
			std::pair<time_t, std::string>* vhost = cmd_acctvhost.vhost.get(entry);
			if(vhost && !vhost->second.empty())
				acct_event.user->ChangeDisplayedHost(vhost->second.c_str());
		}
	}

	virtual void OnUserConnect(LocalUser* user)
	{
		if(!account->IsRegistered(user))
			return;
		AccountDBEntry* entry = db->GetAccount(account->GetAccountName(user), false);
		if(!entry)
			return;
		std::pair<time_t, std::string>* vhost = cmd_acctvhost.vhost.get(entry);
		if(vhost && !vhost->second.empty())
			user->ChangeDisplayedHost(vhost->second.c_str());
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_ModuleInit, PRIORITY_AFTER, ServerInstance->Modules->Find("m_account.so"));
		ServerInstance->Modules->SetPriority(this, I_OnUserConnect, PRIORITY_AFTER, ServerInstance->Modules->Find("m_hostchange.so"));
	}

	Version GetVersion()
	{
		return Version("Allow vhosts to be set on accounts", VF_VENDOR|VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleAccountVhost)
