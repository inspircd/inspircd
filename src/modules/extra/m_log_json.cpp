/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022-2024 Sadie Powell <sadie@witchery.services>
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

/// $CompilerFlags: require_library("yyjson") find_compiler_flags("yyjson") -DHAS_YYJSON
/// $CompilerFlags: require_library("!yyjson") find_compiler_flags("RapidJSON") warning("Building log_json against RapidJSON is deprecated. Please build against yyjson instead.")

/// $LinkerFlags: require_library("yyjson") find_linker_flags("yyjson")

/// $PackageInfo: require_system("alpine") pkgconf yyjson-dev
/// $PackageInfo: require_system("arch") pkgconf yyjson
/// $PackageInfo: require_system("darwin") pkg-config yyjson
/// $PackageInfo: require_system("debian~") rapidjson-dev pkg-config

#ifdef _WIN32
# define HAS_YYJSON
# pragma comment(lib, "yyjson.lib")
#endif

#ifdef HAS_YYJSON
# include <yyjson.h>
#else
# include <rapidjson/ostreamwrapper.h>
# include <rapidjson/writer.h>
#endif

#include "inspircd.h"
#include "timeutils.h"

#include <iostream>
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
	// RapidJSON API: The type of character that this writer accepts.
	typedef char Ch;

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

#ifndef HAS_YYJSON
	// RapidJSON API: We implement our own flushing in OnLog.
	void Flush() { }
#endif

	void OnLog(time_t time, Log::Level level, const std::string& type, const std::string& message) override
	{
		static time_t prevtime = 0;
		static std::string timestr;
		if (prevtime != time)
		{
			prevtime = time;
			timestr = Time::ToString(prevtime, "%Y-%m-%dT%H:%M:%S%z");
		}

		const auto* levelstr = Log::LevelToString(level);

#ifdef HAS_YYJSON
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
			throw CoreException(INSP_FORMAT("Unable to generate JSON for {}: {}", name, errmsg.msg ? errmsg.msg : "unknown error"));
#else
		rapidjson::Writer writer(*this);
		writer.StartObject();
		{
			writer.Key("time", 4);
			writer.String(timestr.c_str(), static_cast<rapidjson::SizeType>(timestr.size()));

			writer.Key("type", 4);
			writer.String(type.c_str(), static_cast<rapidjson::SizeType>(type.size()));

			writer.Key("level", 5);
			writer.String(levelstr, static_cast<rapidjson::SizeType>(strlen(levelstr)));

			writer.Key("message", 7);
			writer.Key(message.c_str(), static_cast<rapidjson::SizeType>(message.size()));
		}
		writer.EndObject();

# ifdef _WIN32
		fputs("\r\n", file);
# else
		fputs("\n", file);
# endif
#endif

		if (!(++lines % flush))
			fflush(file);

		if (ferror(file))
			throw CoreException(INSP_FORMAT("Unable to write to {}: {}", name, strerror(errno)));
	}

#ifndef HAS_YYJSON
	// RapidJSON API: Write a character to the file.
	void Put(Ch c)
	{
		fputc(c, file);
	}
#endif

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
			throw CoreException(INSP_FORMAT("Unable to open {} for JSON logger at {}: {}",
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
#ifdef HAS_YYJSON
		const auto yyversion = yyjson_version();
		ServerInstance->Logs.Normal(MODNAME, "Module was compiled against yyjson version {} and is running against version {}.{}.{}",
			YYJSON_VERSION_STRING, (yyversion >> 16) & 0xFF, (yyversion >> 8) & 0xFF, yyversion & 0xFF);
#else
		ServerInstance->Logs.Normal(MODNAME, "Module was compiled against RapidJSON version {}",
			RAPIDJSON_VERSION_STRING);
#endif
	}
};

MODULE_INIT(ModuleLogJSON)
