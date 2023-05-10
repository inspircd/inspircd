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
#include "timeutils.h"

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
		, Stats::EventListener(this, UINT_MAX)
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
		const auto& security = ServerInstance->Config->ConfValue("security");
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
		if (InspIRCd::MatchMask(hosts, user->GetRealUserHost(), user->GetUserAddress()))
			return MOD_RES_PASSTHRU; // Host matches.

		if (!automatic)
		{
			ServerInstance->SNO.WriteGlobalSno('o', "{} ({}) [{}] failed to log into the \x02{}\x02 oper account because they are connecting from the wrong user@host.",
				user->nick, user->GetRealUserHost(), user->GetAddress(), oper->GetName());
		}
		return MOD_RES_DENY; // Host does not match.
	}

	void OnPostOperLogin(User* user, bool automatic) override
	{
		LocalUser* luser = IS_LOCAL(user);
		if (!luser)
			return;

		luser->WriteNumeric(RPL_YOUAREOPER, INSP_FORMAT("You are now {} {}", strchr("AEIOUaeiou", user->oper->GetType()[0]) ? "an" : "a",
			user->oper->GetType()));

		ServerInstance->SNO.WriteToSnoMask('o', "{} ({}) [{}] is now a server operator of type \x02{}\x02 ({}using account \x02{}\x02).",
			user->nick, user->GetRealUserHost(), user->GetAddress(), user->oper->GetType(),
			automatic ? "automatically " : "", user->oper->GetName());

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

					stats.AddGenericRow(INSP_FORMAT(
						"\x02{}\x02 (hosts: {}, type: {}, channel modes: {}, user modes: {}, snomasks: {}, commands: {}, privileges: {})",
						account->GetName(), NoneIfEmpty(hosts), account->GetType(), NoneIfEmpty(chanmodes), NoneIfEmpty(usermodes),
						NoneIfEmpty(snomasks), NoneIfEmpty(commands), NoneIfEmpty(privileges)
					))
					.AddTags(stats, {
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

					stats.AddGenericRow(INSP_FORMAT(
						"\x02{}\02 (channel modes: {}, user modes: {}, snomasks: {}, commands: {}, privileges: {})",
						type->GetName(), NoneIfEmpty(chanmodes), NoneIfEmpty(usermodes), NoneIfEmpty(snomasks),
						NoneIfEmpty(commands), NoneIfEmpty(privileges)
					))
					.AddTags(stats, {
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
				for (auto* oper : ServerInstance->Users.all_opers)
				{
					if (oper->server->IsService())
						continue;

					std::string extra;
					if (oper->IsAway())
					{
						const std::string awayperiod = Duration::ToString(ServerInstance->Time() - oper->awaytime);
						const std::string awaytime = Time::ToString(oper->awaytime);

						extra = INSP_FORMAT(": away for {} [since {}] ({})", awayperiod, awaytime, oper->awaymsg);
					}

					auto* loper = IS_LOCAL(oper);
					if (loper)
					{
						const std::string idleperiod = Duration::ToString(ServerInstance->Time() - loper->idle_lastmsg);
						const std::string idletime = Time::ToString(loper->idle_lastmsg);

						extra += INSP_FORMAT("{} idle for {} [since {}]",  extra.empty() ? ':' : ',', idleperiod, idletime);
					}

					stats.AddGenericRow(INSP_FORMAT("\x02{}\x02 ({}){}", oper->nick, oper->GetRealUserHost(), extra));
				}

				// Sort opers alphabetically.
				std::sort(stats.GetRows().begin(), stats.GetRows().end(), [](const auto& lhs, const auto& rhs) {
					return lhs.GetParams()[1] < rhs.GetParams()[1];
				});

				stats.AddGenericRow(INSP_FORMAT("{} server operator{} total", stats.GetRows().size(), stats.GetRows().size() ? "s" : ""));
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(CoreModOper)
