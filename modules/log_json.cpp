/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022-2025 Sadie Powell <sadie@witchery.services>
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

/// $CompilerFlags: require_environment("SYSTEM_YYJSON" "1") find_compiler_flags("yyjson") -DUSE_SYSTEM_YYJSON
/// $LinkerFlags: require_environment("SYSTEM_YYJSON" "1")  find_linker_flags("yyjson")


#ifdef USE_SYSTEM_YYJSON
# include <yyjson.h>
#else
# include <yyjson/yyjson.c>
#endif

#include "inspircd.h"
#include "timeutils.h"

class JSONMethod final
	: public Log::Method
	, public Timer
{
private:
	// Whether to autoclose the file on exit.
	bool autoclose;

	// The file to which the log is written.
	FILE* file;

	// How often the file stream should be flushed.
	const unsigned long flush;

	// The number of lines which have been written since the file stream was created.
	unsigned long lines = 0;

	/// The name the underlying file.
	const std::string name;

public:
	JSONMethod(const std::string& n, FILE* fh, unsigned long fl, bool ac) ATTR_NOT_NULL(3)
		: Timer(15*60, true)
		, autoclose(ac)
		, file(fh)
		, flush(fl)
		, name(n)
	{
		if (flush > 1)
			ServerInstance->Timers.AddTimer(this);
	}

	~JSONMethod() override
	{
		if (autoclose)
			fclose(file);
	}

	void OnLog(time_t time, Log::Level level, const std::string& type, const std::string& message) override
	{
		static time_t prevtime = 0;
		static std::string timestr;
		if (prevtime != time)
		{
			prevtime = time;
			timestr = Time::ToString(prevtime, Time::ISO_8601);
		}

		const auto* levelstr = Log::LevelToString(level);

		auto* doc = yyjson_mut_doc_new(nullptr);

		auto* root = yyjson_mut_obj(doc);
		yyjson_mut_doc_set_root(doc, root);

		auto error = false;
		error |= yyjson_mut_obj_add_strn(doc, root, "time", timestr.c_str(), timestr.length());
		error |= yyjson_mut_obj_add_strn(doc, root, "type", type.c_str(), type.length());
		error |= yyjson_mut_obj_add_strn(doc, root, "level", levelstr, strlen(levelstr));
		error |= yyjson_mut_obj_add_strn(doc, root, "message", message.c_str(), message.length());

		yyjson_write_err errmsg;
		error |= yyjson_mut_write_fp(file, doc, YYJSON_WRITE_ALLOW_INVALID_UNICODE | YYJSON_WRITE_NEWLINE_AT_END, nullptr, &errmsg);

		yyjson_mut_doc_free(doc);

		if (error)
			throw CoreException(FMT::format("Unable to generate JSON for {}: {}", name, errmsg.msg ? errmsg.msg : "unknown error"));

		if (!(++lines % flush))
			fflush(file);

		if (ferror(file))
			throw CoreException(FMT::format("Unable to write to {}: {}", name, strerror(errno)));
	}

	bool Tick() override
	{
		fflush(file);
		return true;
	}
};

class JSONFileEngine final
	: public Log::Engine
{
public:
	JSONFileEngine(Module* Creator) ATTR_NOT_NULL(2)
		: Log::Engine(Creator, "json")
	{
	}

	Log::MethodPtr Create(const std::shared_ptr<ConfigTag>& tag) override
	{
		const std::string target = tag->getString("target");
		if (target.empty())
			throw CoreException("<log:target> must be specified for JSON logger at " + tag->source.str());

		const std::string fulltarget = ServerInstance->Config->Paths.PrependLog(Time::ToString(ServerInstance->Time(), target.c_str()));
		auto* fh = fopen(fulltarget.c_str(), "a");
		if (!fh)
		{
			throw CoreException(FMT::format("Unable to open {} for JSON logger at {}: {}",
				fulltarget, tag->source.str(), strerror(errno)));
		}

		const unsigned long flush = tag->getNum<unsigned long>("flush", 20, 1);
		return std::make_shared<JSONMethod>(fulltarget, fh, flush, true);
	}
};

class JSONStreamEngine final
	: public Log::Engine
{
private:
	FILE* file;

public:
	JSONStreamEngine(Module* Creator, const std::string& Name, FILE* fh) ATTR_NOT_NULL(2, 4)
		: Log::Engine(Creator, Name)
		, file(fh)
	{
	}

	Log::MethodPtr Create(const std::shared_ptr<ConfigTag>& tag) override
	{
		return std::make_shared<JSONMethod>(name, file, 1, false);
	}
};

class ModuleLogJSON final
	: public Module
{
private:
	JSONFileEngine log;
	JSONStreamEngine stderrlog;
	JSONStreamEngine stdoutlog;

public:
	ModuleLogJSON()
		: Module(VF_VENDOR, "Provides the ability to log to JSON.")
		, log(this)
		, stderrlog(this, "json-stderr", stderr)
		, stdoutlog(this, "json-stdout", stdout)
	{
	}

	void init() override
	{
		const auto yyversion = yyjson_version();
		ServerInstance->Logs.Normal(MODNAME, "Module was compiled against yyjson version {} and is running against version {}.{}.{}",
			YYJSON_VERSION_STRING, (yyversion >> 16) & 0xFF, (yyversion >> 8) & 0xFF, yyversion & 0xFF);
	}
};

MODULE_INIT(ModuleLogJSON)
