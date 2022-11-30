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

	void OnPostOperLogin(User* user) override
	{
		LocalUser* luser = IS_LOCAL(user);
		if (!luser)
			return;

		luser->WriteNumeric(RPL_YOUAREOPER, InspIRCd::Format("You are now %s %s",
			strchr("AEIOUaeiou", user->oper->GetType()[0]) ? "an" : "a",
			user->oper->GetType().c_str()));

		ServerInstance->SNO.WriteToSnoMask('o', "%s (%s) is now a server operator of type %s (using account %s)",
			user->nick.c_str(), user->MakeHost().c_str(), user->oper->GetType().c_str(),
			user->oper->GetName().c_str());

		const std::string vhost = luser->oper->GetConfig()->getString("vhost");
		if (!vhost.empty())
			user->ChangeDisplayedHost(vhost);

		const std::string klass = luser->oper->GetConfig()->getString("class");
		if (!klass.empty())
			luser->SetClass(klass);
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
					auto loper = IS_LOCAL(oper);
					if (loper)
					{
						const std::string idleperiod = InspIRCd::DurationString(ServerInstance->Time() - loper->idle_lastmsg);
						const std::string idletime = InspIRCd::TimeString(ServerInstance->Time());
						stats.AddGenericRow(InspIRCd::Format("\x02%s\x02 (%s): idle for %s [since %s]", oper->nick.c_str(),
							oper->MakeHost().c_str(), idleperiod.c_str(), idletime.c_str()));
					}
					else
					{
						stats.AddGenericRow(InspIRCd::Format("\x02%s\x02 (%s)", oper->nick.c_str(), oper->MakeHost().c_str()));
					}
				}
				stats.AddGenericRow(InspIRCd::Format("%zu server operator%s total", opers, opers ? "s" : ""));
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(CoreModOper)
