/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 Sadie Powell <sadie@witchery.services>
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

#include <syslog.h>

class SyslogMethod final
	: public Log::Method
{
private:
	// Converts an InspIRCd log level to syslog priority.
	static int LevelToPriority(Log::Level level)
	{
		switch (level)
		{
			case Log::Level::CRITICAL:
				return LOG_ERR;

			case Log::Level::WARNING:
				return LOG_WARNING;

			case Log::Level::NORMAL:
				return LOG_NOTICE;

			case Log::Level::DEBUG:
			case Log::Level::RAWIO:
				return LOG_DEBUG;
		}

		// Should never happen.
		return LOG_NOTICE;
	}

public:
	void OnLog(time_t time, Log::Level level, const std::string& type, const std::string& message) override
	{
		syslog(LevelToPriority(level), "%s: %s", type.c_str(), message.c_str());
	}
};

class SyslogEngine final
	: public Log::Engine
{
public:
	SyslogEngine(Module* Creator) ATTR_NOT_NULL(2)
		: Log::Engine(Creator, "syslog")
	{
	}

	Log::MethodPtr Create(const std::shared_ptr<ConfigTag>& tag) override
	{
		return std::make_shared<SyslogMethod>();
	}
};

class ModuleLogSyslog final
	: public Module
{
private:
	SyslogEngine engine;

public:
	ModuleLogSyslog()
		: Module(VF_VENDOR, "Provides the ability to log to syslog.")
		, engine(this)
	{
		openlog("inspircd", LOG_NDELAY|LOG_PID, LOG_USER);
	}

	~ModuleLogSyslog() override
	{
		closelog();
	}
};

MODULE_INIT(ModuleLogSyslog)
