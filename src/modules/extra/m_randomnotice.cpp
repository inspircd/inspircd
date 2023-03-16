/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2003, 2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2005 Craig McLure <craig@chatspike.net>
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

/// $ModAuthor: genius3000
/// $ModAuthorMail: genius3000@g3k.solutions
/// $ModConfig: <randomnotice file="randomnotices.txt" interval="30m" prefix="" suffix="">
/// $ModDepends: core 3
/// $ModDesc: Send a random notice (quote) from a file to all users at a set interval.
// "file" needs to be a text file with each 'notice' on a new line
// "interval" is a time-string (1y7d8h6m3s format)


#include "inspircd.h"

class RandomNoticeTimer : public Timer
{
 public:
	std::vector<std::string> notices;
	std::string prefix;
	std::string suffix;

	RandomNoticeTimer() : Timer(1800, true) { }

	bool Tick(time_t) CXX11_OVERRIDE
	{
		if (notices.empty())
			return false;

		unsigned long random = ServerInstance->GenRandomInt(notices.size());
		const std::string& notice = notices[random];

		for (UserManager::LocalList::const_iterator i = ServerInstance->Users.GetLocalUsers().begin(); i != ServerInstance->Users.GetLocalUsers().end(); ++i)
		{
			LocalUser* user = *i;

			if (user->registered == REG_ALL)
				user->WriteNotice(prefix + notice + suffix);
		}

		return true;
	}
};

class ModuleRandomNotice : public Module
{
	RandomNoticeTimer* timer;

 public:
	ModuleRandomNotice()
	{
		timer = new RandomNoticeTimer();
	}

	~ModuleRandomNotice()
	{
		ServerInstance->Timers.DelTimer(timer);
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->Timers.AddTimer(timer);
	}

	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("randomnotice");
		FileReader reader(tag->getString("file", "randomnotices.txt"));
		timer->notices = reader.GetVector();
		timer->prefix = tag->getString("prefix");
		timer->suffix = tag->getString("suffix");
		unsigned long interval = tag->getDuration("interval", 1800, 60, 31536000);
		if (timer->GetInterval() != interval)
			timer->SetInterval(interval);

		if (timer->notices.empty())
			throw ModuleException("Random Notices file is empty!! Please add quotes to the file.");
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Send a random notice (quote) to all users at a set interval.");
	}
};

MODULE_INIT(ModuleRandomNotice)
