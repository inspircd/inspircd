/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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
#include "modules/stats.h"

#include "core_oper.h"

class CoreModOper final
	: public Module
	, public Stats::EventListener
{
private:
	CommandDie cmddie;
	CommandKill cmdkill;
	CommandOper cmdoper;
	CommandRehash cmdrehash;
	CommandRestart cmdrestart;
	ModeUserOperator operatormode;
	ModeUserServerNoticeMask snomaskmode;

	static std::string NoneIfEmpty(const std::string& field)
	{
		return field.empty() ? "\x1Dnone\x1D" : field;
	}

public:
	CoreModOper()
		: Module(VF_CORE | VF_VENDOR, "Provides the DIE, KILL, OPER, REHASH, and RESTART commands")
		, Stats::EventListener(this)
		, cmddie(this)
		, cmdkill(this)
		, cmdoper(this)
		, cmdrehash(this)
		, cmdrestart(this)
		, operatormode(this)
		, snomaskmode(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		auto security = ServerInstance->Config->ConfValue("security");
		cmdkill.hidenick = security->getString("hidekills");
		cmdkill.hideservicekills = security->getBool("hideservicekills", security->getBool("hideulinekills"));
	}

	void OnPostConnect(User* user) override
	{
		LocalUser* luser = IS_LOCAL(user);
		if (!luser)
			return;

		// Find an auto-oper block for this user.
		for (const auto& [_, account] :  ServerInstance->Config->OperAccounts)
		{
			if (!account->CanAutoLogin(luser))
				continue; // No autologin for this account.

			if (user->OperLogin(account, true))
				break; // Successfully logged in to the account.
		}
	}

	ModResult OnPreOperLogin(LocalUser* user, const std::shared_ptr<OperAccount>& oper, bool automatic) override
	{
		const std::string hosts = oper->GetConfig()->getString("host");
		if (InspIRCd::MatchMask(hosts, user->MakeHost(), user->MakeHostIP()))
			return MOD_RES_PASSTHRU; // Host matches.

		if (!automatic)
		{
			ServerInstance->SNO.WriteGlobalSno('o', "%s (%s) [%s] failed to log into the \x02%s\x02 oper account because they are connecting from the wrong user@host.",
				user->nick.c_str(), user->MakeHost().c_str(), user->GetIPString().c_str(), oper->GetName().c_str());
		}
		return MOD_RES_DENY; // Host does not match.
	}

	void OnPostOperLogin(User* user, bool automatic) override
	{
		LocalUser* luser = IS_LOCAL(user);
		if (!luser)
			return;

		luser->WriteNumeric(RPL_YOUAREOPER, InspIRCd::Format("You are now %s %s",
			strchr("AEIOUaeiou", user->oper->GetType()[0]) ? "an" : "a",
			user->oper->GetType().c_str()));

		ServerInstance->SNO.WriteToSnoMask('o', "%s (%s) [%s] is now a server operator of type \x02%s\x02 (%susing account \x02%s\x02).",
			user->nick.c_str(), user->MakeHost().c_str(), user->GetIPString().c_str(), user->oper->GetType().c_str(),
			automatic ? "automatically " : "", user->oper->GetName().c_str());

		const std::string vhost = luser->oper->GetConfig()->getString("vhost");
		if (!vhost.empty())
			user->ChangeDisplayedHost(vhost);

		const std::string klassname = luser->oper->GetConfig()->getString("class");
		if (!klassname.empty())
		{
			for (const auto& klass : ServerInstance->Config->Classes)
			{
				if (klassname == klass->GetName())
				{
					luser->ChangeConnectClass(klass, true);
					break;
				}
			}
		}
	}

	ModResult OnStats(Stats::Context& stats) override
	{
		switch (stats.GetSymbol())
		{
			case 'o':
			{
				// Server operator accounts.
				for (const auto& [_, account] : ServerInstance->Config->OperAccounts)
				{
					const std::string hosts = account->GetConfig()->getString("host");
					const std::string chanmodes = account->GetModes(MODETYPE_CHANNEL);
					const std::string usermodes = account->GetModes(MODETYPE_USER);
					const std::string snomasks = account->GetSnomasks();
					const std::string commands = account->GetCommands();
					const std::string privileges = account->GetPrivileges();

					stats.AddGenericRow(InspIRCd::Format(
						"\x02%s\x02 (hosts: %s, type: %s, channel modes: %s, user modes: %s, snomasks: %s, commands: %s, privileges: %s)",
						account->GetName().c_str(), NoneIfEmpty(hosts).c_str(), account->GetType().c_str(),
						NoneIfEmpty(chanmodes).c_str(), NoneIfEmpty(usermodes).c_str(),
						NoneIfEmpty(snomasks).c_str(), NoneIfEmpty(commands).c_str(),
						NoneIfEmpty(privileges).c_str())
					).AddTags(stats, {
						{ "name",       account->GetName() },
						{ "hosts",      hosts              },
						{ "chan-modes", chanmodes          },
						{ "user-modes", usermodes          },
						{ "snomasks",   snomasks           },
						{ "commands",   commands           },
						{ "privileges", privileges         },
					});
				}
				return MOD_RES_DENY;
			}

			case 'O':
			{
				// Server operator types.
				for (const auto& [_, type] : ServerInstance->Config->OperTypes)
				{
					const std::string chanmodes = type->GetModes(MODETYPE_CHANNEL);
					const std::string usermodes = type->GetModes(MODETYPE_USER);
					const std::string snomasks = type->GetSnomasks();
					const std::string commands = type->GetCommands();
					const std::string privileges = type->GetPrivileges();

					stats.AddGenericRow(InspIRCd::Format(
						"\x02%s\02 (channel modes: %s, user modes: %s, snomasks: %s, commands: %s, privileges: %s)",
						type->GetName().c_str(), NoneIfEmpty(chanmodes).c_str(),
						NoneIfEmpty(usermodes).c_str(), NoneIfEmpty(snomasks).c_str(),
						NoneIfEmpty(commands).c_str(), NoneIfEmpty(privileges).c_str())
					).AddTags(stats, {
						{ "name",       type->GetName()  },
						{ "chan-modes", chanmodes        },
						{ "user-modes", usermodes        },
						{ "snomasks",   snomasks         },
						{ "commands",   commands         },
						{ "privileges", privileges       },
					});
				}
				return MOD_RES_DENY;
			}

			case 'P':
			{
				// Online server operators.
				size_t opers = 0;
				for (const auto& oper : ServerInstance->Users.all_opers)
				{
					if (oper->server->IsService())
						continue;

					opers++;
					std::string extra;
					if (oper->IsAway())
					{
						const std::string awayperiod = InspIRCd::DurationString(ServerInstance->Time() - oper->awaytime);
						const std::string awaytime = InspIRCd::TimeString(oper->awaytime);
						extra += InspIRCd::Format(": away for %s [since %s] (%s)", awayperiod.c_str(),
							awaytime.c_str(), oper->awaymsg.c_str());
					}

					auto loper = IS_LOCAL(oper);
					if (loper)
					{
						const std::string idleperiod = InspIRCd::DurationString(ServerInstance->Time() - loper->idle_lastmsg);
						const std::string idletime = InspIRCd::TimeString(loper->idle_lastmsg);
						extra += InspIRCd::Format("%c idle for %s [since %s]",  extra.empty() ? ':' : ',',
							idleperiod.c_str(), idletime.c_str());
					}

					stats.AddGenericRow(InspIRCd::Format("\x02%s\x02 (%s)%s", oper->nick.c_str(),
						oper->MakeHost().c_str(), extra.c_str()));
				}
				stats.AddGenericRow(InspIRCd::Format("%zu server operator%s total", opers, opers ? "s" : ""));
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(CoreModOper)
