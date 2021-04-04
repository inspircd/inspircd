/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
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

#include "event.h"

namespace Regex
{
	class Engine;
	class EngineReference;
	class Exception;
	class Pattern;
	template<typename> class SimpleEngine;

	/** A shared pointer to a regex pattern. */
	typedef std::shared_ptr<Pattern> PatternPtr;

	/** The options to use when matching a pattern. */
	enum PatternOptions : uint8_t
	{
		/** No special matching options apply. */
		OPT_NONE = 0,

		/** The pattern is case insensitive. */
		OPT_CASE_INSENSITIVE = 1,
	};
}

/** The base class for regular expression engines. */
class Regex::Engine
	: public DataProvider
{
 protected:
	/** Initializes a new instance of the Regex::Engine class.
	 * @param Creator The module which created this instance.
	 * @param Name The name of this regular expression engine.
	 */
	Engine(Module* Creator, const std::string& Name)
		: DataProvider(Creator, "regex/" + Name)
	{
	}

 public:
	/** Compiles a regular expression pattern.
	 * @param pattern The pattern to compile.
	 * @param options One or more options to use when matching the pattern.
	 * @return A shared pointer to an instance of the Regex::Pattern class.
	 */
	virtual PatternPtr Create(const std::string& pattern, uint8_t options = Regex::OPT_NONE) = 0;

	/** Compiles a regular expression from the human-writable form.
	 * @param pattern The pattern to compile in the format /pattern/flags.
	 * @return A shared pointer to an instance of the Regex::Pattern class.
	 */
	PatternPtr CreateHuman(const std::string& pattern);
};

/**The base class for simple regular expression engines. */
template<typename PatternClass>
class Regex::SimpleEngine final
	: public Regex::Engine
{
 public:
 	/** @copydoc Regex::Engine::Engine */
	SimpleEngine(Module* Creator, const std::string& Name)
		: Regex::Engine(Creator, Name)
	{
	}

	/** @copydoc Regex::Engine::Create */
	PatternPtr Create(const std::string& pattern, uint8_t options) override
	{
		return std::make_shared<PatternClass>(pattern, options);
	}
};

/** A dynamic reference to an instance of the Regex::Engine class. */
class Regex::EngineReference final
	: public dynamic_reference_nocheck<Engine>
{
 public:
	/** Initializes a new instance of the Regex::EngineReference class.
	 * @param Creator The module which created this instance.
	 * @param Name The name of the regular expression engine to reference.
	 */
	EngineReference(Module* Creator, const std::string& Name = "")
		: dynamic_reference_nocheck<Engine>(Creator, Name.empty() ? "regex" : "regex/" + Name)
	{
	}

	/** Sets the name of the engine this reference is configured with.
	 * @param engine The name of the engine to refer to.
	 */
	void SetEngine(const std::string& engine)
	{
		SetProvider(engine.empty() ? "regex" : "regex/" + engine);
	}
};

/** The exception which is thrown when a regular expression fails to compile. */
class Regex::Exception final
	: public ModuleException
{
 public:
	/** Initializes a new instance of the Regex::Exception class.
	 * @param regex A regular expression which failed to compile.
	 * @param error The error which occurred whilst compiling the regular expression.
	*/
	Exception(const std::string& regex, const std::string& error)
		: ModuleException("Error in regex '" + regex + "': " + error)
	{
	}

	/** Initializes a new instance of the Regex::Exception class.
	 * @param regex A regular expression which failed to compile.
	 * @param error The error which occurred whilst compiling the regular expression.
	 * @param offset The offset at which the errror occurred.
	*/
	Exception(const std::string& regex, const std::string& error, int offset)
		: ModuleException("Error in regex '" + regex + "' at offset " + ConvToStr(offset) + ": " + error)
	{
	}
};

/** Represents a compiled regular expression pattern. */
class Regex::Pattern
{
 private:
	/** The options used when matching this pattern. */
	const uint8_t optionflags;

	/** The pattern as a string. */
	const std::string patternstr;

 protected:
	/** Initializes a new instance of the Pattern class.
	 * @param pattern The pattern as a string.
	 * @param options The options used when matching this pattern.
	 */
	Pattern(const std::string& pattern, uint8_t options)
		: optionflags(options)
		, patternstr(pattern)
	{
	}

 public:
	/** Destroys an instance of the Pattern class. */
	virtual ~Pattern() = default;

	/** Retrieves the options used when matching this pattern. */
	uint8_t GetOptions() const { return optionflags; }

	/** Retrieves the pattern as a string. */
	const std::string& GetPattern() const { return patternstr; }

	/** Attempts to match this pattern against the specified text.
	 * @param text The text to match against.
	 * @return If the text matched the pattern then true; otherwise, false.
	 */
	virtual bool IsMatch(const std::string& text) = 0;
};

inline Regex::PatternPtr Regex::Engine::CreateHuman(const std::string& pattern)
{
	if (pattern.empty() || pattern[0] != '/')
		return Create(pattern, OPT_NONE);

	size_t end = pattern.find_last_not_of("Ii");
	if (!end || end == std::string::npos || pattern[end] != '/')
		throw Exception(pattern, "Regex patterns must be terminated with a '/'!");

	uint8_t options = Regex::OPT_NONE;
	for (std::string::const_iterator iter = pattern.begin() + end + 1; iter != pattern.end(); ++iter)
	{
		switch (*iter)
		{
			case 'I':
			case 'i':
				options |= OPT_CASE_INSENSITIVE;
				break;
		}
	}
	return Create(pattern.substr(1, end - 1), options);
}
