/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
#include "core_oper.h"

class CoreModOper : public Module
{
	CommandDie cmddie;
	CommandKill cmdkill;
	CommandOper cmdoper;
	CommandRehash cmdrehash;
	CommandRestart cmdrestart;

 public:
	CoreModOper()
		: cmddie(this)
		, cmdkill(this)
		, cmdoper(this)
		, cmdrehash(this)
		, cmdrestart(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		ConfigTag* security = ServerInstance->Config->ConfValue("security");
		cmdkill.hidenick = security->getString("hidekills");
		cmdkill.hideuline = security->getBool("hideulinekills");
	}

	void OnPostOper(User* user, const std::string&, const std::string&) override
	{
		LocalUser* luser = IS_LOCAL(user);
		if (!luser)
			return;

		const std::string vhost = luser->oper->getConfig("vhost");
		if (!vhost.empty())
			luser->ChangeDisplayedHost(vhost);

		const std::string klass = luser->oper->getConfig("class");
		if (!klass.empty())
			luser->SetClass(klass);
	}

	Version GetVersion() override
	{
		return Version("Provides the DIE, KILL, OPER, REHASH, and RESTART commands", VF_VENDOR | VF_CORE);
	}
};

MODULE_INIT(CoreModOper)
