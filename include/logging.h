/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022-2023 Sadie Powell <sadie@witchery.services>
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


#pragma once

namespace Log
{
	class Method;
	class FileMethod;

	class Engine;
	class FileEngine;
	class StreamEngine;

	class Manager;

	/** A shared pointer to a log method. */
	typedef std::shared_ptr<Method> MethodPtr;

	/** Levels at which messages can be logged. */
	enum class Level
		: uint8_t
	{
		/** A critical message which must be investigated. */
		CRITICAL = 0,

		/** An important message which should be investigated. */
		WARNING = 1,

		/** A general message which is useful to have on record. */
		NORMAL = 2,

		/** A debug message that we might want to store when testing. */
		DEBUG = 3,

		/** A sensitive message that we should not store lightly. */
		RAWIO = 4,
	};

	/** Converts a log level to a string.
	 * @param level The log level to convert.
	 */
	CoreExport const char* LevelToString(Level level);

	/** Notify a user that raw I/O logging is enabled.
	 * @param user The user to notify.
	 * @param type The type of message to send.
	 */
	CoreExport void NotifyRawIO(LocalUser* user, MessageType type);
}

/** Base class for logging methods. */
class CoreExport Log::Method
{
protected:
	Method() = default;

public:
	virtual ~Method() = default;

	/** Determines whether this logging method accepts cached messages. */
	virtual bool AcceptsCachedMessages() const { return true; }

	/** Writes a message to the logger.
	 * @param time The time at which the message was logged.
	 * @param level The level at which the log message was written.
	 * @param type The component which wrote the log message.
	 * @param message The message which was written to the log.
	 */
	virtual void OnLog(time_t time, Level level, const std::string& type, const std::string& message) = 0;
};

/** A logger that writes to a file stream. */
class CoreExport Log::FileMethod final
	: public Method
	, public Timer
{
private:
	/** Whether to autoclose the file on exit. */
	bool autoclose;

	/** The file to which the log is written. */
	FILE* file;

	/** How often the file stream should be flushed. */
	const unsigned long flush;

	/** The number of lines which have been written since the file stream was created. */
	unsigned long lines = 0;

	/** The name the underlying file. */
	const std::string name;

public:
	FileMethod(const std::string& n, FILE* fh, unsigned long fl, bool ac);
	~FileMethod() override;

	/** @copydoc Log::Method::AcceptsCachedMessages */
	bool AcceptsCachedMessages() const override { return false; }

	/** @copydoc Timer::Tick */
	bool Tick() override;

	/** @copydoc Log::Method::OnLog */
	void OnLog(time_t time, Level level, const std::string& type, const std::string& message) override;
};

/** Base class for logging engines. */
class CoreExport Log::Engine
	: public DataProvider
{
protected:
	Engine(Module* Creator, const std::string& Name);

public:
	virtual ~Engine() override;

	/** Creates a new logger from the specified config.
	 * @param tag The config tag to configure the logger with.
	 */
	virtual MethodPtr Create(const std::shared_ptr<ConfigTag>& tag) = 0;

};

/** A logger which writes to a file. */
class CoreExport Log::FileEngine final
	: public Engine
{
public:
	FileEngine(Module* Creator);

	/** @copydoc Log::Engine::Create */
	MethodPtr Create(const std::shared_ptr<ConfigTag>& tag) override;
};

/** A logger which writes to a stream. */
class CoreExport Log::StreamEngine final
	: public Engine
{
private:
	FILE* file;

public:
	StreamEngine(Module* Creator, const std::string& Name, FILE* fh);

	/** @copydoc Log::Engine::Create */
	MethodPtr Create(const std::shared_ptr<ConfigTag>& tag) override;
};

/** Manager for the logging system. */
class CoreExport Log::Manager final
{
private:
	/** A log message which has been cached until modules load. */
	struct CachedMessage final
	{
		/** The time the message was logged at. */
		time_t time;

		/** The level the message was logged at. */
		Level level;

		/** The type of the message that was logged. */
		std::string type;

		/** The message that was logged. */
		std::string message;

		CachedMessage(time_t ts, Level l, const std::string& t, const std::string& m);
	};

	/** Encapsulates information about a logger. */
	struct Info final
	{
		/** Whether the logger was read from the server config. */
		bool config;

		/** The minimum log level that this logger accepts. */
		Level level;

		/** The types of log message that this logger accepts. */
		TokenList types;

		/** The handler for this logger type. */
		MethodPtr method;

		/** The engine which created this logger. */
		const Engine* engine;

		Info(Level l, TokenList t, MethodPtr m, bool c, const Engine* e) ATTR_NOT_NULL(6);
	};

	/** The log messages we have cached for modules. */
	std::vector<CachedMessage> cache;

	/** Whether we have just started up and need to cache messages until modules are loaded. */
	bool caching = true;

	/** The logger engine that writes to a file. */
	Log::FileEngine filelog;

	/** A logger that writes to stderr. */
	Log::StreamEngine stderrlog;

	/** A logger that writes to stdout. */
	Log::StreamEngine stdoutlog;

	/** The currently registered loggers. */
	std::vector<Info> loggers;

	/** Whether we are currently logging to a file. */
	bool logging = false;

	/** Writes a message to the server log.
	 * @param level The level to log at.
	 * @param type The type of message that is being logged.
	 * @param message The message to log.
	 */
	void Write(Level level, const std::string& type, const std::string& message);

public:
	Manager();

	/** Closes all loggers which were opened from the config. */
	void CloseLogs();

	/** Enables writing rawio logs to the standard output stream. */
	void EnableDebugMode();

	/** Opens loggers that are specified in the config. */
	void OpenLogs(bool requiremethods);

	/** Registers the core logging services with the event system. */
	void RegisterServices();

	/** Unloads all loggers that are provided by the specified engine.
	 * @param engine The engine to unload the loggers of.
	 */
	void UnloadEngine(const Engine* engine) ATTR_NOT_NULL(2);

	/** Writes an critical message to the server log.
	 * @param type The type of message that is being logged.
	 * @param message The message to log.
	 */
	inline void Critical(const std::string& type, const std::string& message)
	{
		Write(Level::CRITICAL, type, message);
	}

	/** Writes an critical message to the server log.
	 * @param type The type of message that is being logged.
	 * @param format A format string to format and then log.
	 * @param args One or more arguments to format the string with.
	 */
	template <typename... Args>
	void Critical(const std::string& type, const char* format, Args&&... args)
	{
		Write(Level::CRITICAL, type, fmt::format(fmt::runtime(format), std::forward<Args>(args)...));
	}

	/** Writes a warning message to the server log.
	 * @param type The type of message that is being logged.
	 * @param message The message to log.
	 */
	inline void Warning(const std::string& type, const std::string& message)
	{
		Write(Level::WARNING, type, message);
	}

	/** Writes a warning message to the server log.
	 * @param type The type of message that is being logged.
	 * @param format A format string to format and then log.
	 * @param args One or more arguments to format the string with.
	 */
	template <typename... Args>
	void Warning(const std::string& type, const char* format, Args&&... args)
	{
		Write(Level::WARNING, type, fmt::format(fmt::runtime(format), std::forward<Args>(args)...));
	}

	/** Writes a normal message to the server log.
	 * @param type The type of message that is being logged.
	 * @param message The message to log.
	 */
	inline void Normal(const std::string& type, const std::string& message)
	{
		Write(Level::NORMAL, type, message);
	}

	/** Writes a normal message to the server log.
	 * @param type The type of message that is being logged.
	 * @param format A format string to format and then log.
	 * @param args One or more arguments to format the string with.
	 */
	template <typename... Args>
	void Normal(const std::string& type, const char* format, Args&&... args)
	{
		Write(Level::NORMAL, type, fmt::format(fmt::runtime(format), std::forward<Args>(args)...));
	}

	/** Writes a debug message to the server log.
	 * @param type The type of message that is being logged.
	 * @param message The message to log.
	 */
	inline void Debug(const std::string& type, const std::string& message)
	{
		Write(Level::DEBUG, type, message);
	}
	/** Writes a debug message to the server log.
	 * @param type The type of message that is being logged.
	 * @param format A format string to format and then log.
	 * @param args One or more arguments to format the string with.
	 */
	template <typename... Args>
	void Debug(const std::string& type, const char* format, Args&&... args)
	{
		Write(Level::DEBUG, type, fmt::format(fmt::runtime(format), std::forward<Args>(args)...));
	}

	/** Writes a raw I/O message to the server log.
	 * @param type The type of message that is being logged.
	 * @param message The message to log.
	 */
	inline void RawIO(const std::string& type, const std::string& message)
	{
		Write(Level::RAWIO, type, message);
	}

	/** Writes a raw I/O message to the server log.
	 * @param type The type of message that is being logged.
	 * @param format A format string to format and then log.
	 * @param args One or more arguments to format the string with.
	 */
	template <typename... Args>
	void RawIO(const std::string& type, const char* format, Args&&... args)
	{
		Write(Level::RAWIO, type, fmt::format(fmt::runtime(format), std::forward<Args>(args)...));
	}
};
