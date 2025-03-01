/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2020-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@gmail.com>
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

namespace Regex
{
	class Engine;
	class EngineReference;
	class Exception;
	class MatchCollection;
	class Pattern;
	template<typename> class SimpleEngine;

	/** A list of matches that were captured by index. */
	typedef std::vector<std::string> Captures;

	/** A list of matches that were captured by name. */
	typedef insp::flat_map<std::string, std::string> NamedCaptures;

	/** A shared pointer to a regex pattern. */
	typedef std::shared_ptr<Pattern> PatternPtr;

	/** The options to use when matching a pattern. */
	enum PatternOptions
		: uint8_t
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
	virtual PatternPtr Create(const std::string& pattern, uint8_t options = Regex::OPT_NONE) const = 0;

	/** Compiles a regular expression from the human-writable form.
	 * @param pattern The pattern to compile in the format /pattern/flags.
	 * @return A shared pointer to an instance of the Regex::Pattern class.
	 */
	PatternPtr CreateHuman(const std::string& pattern) const;

	/** Retrieves the name of this regex engine. */
	const char* GetName() const
	{
		return name.c_str() + 6;
	}
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
	PatternPtr Create(const std::string& pattern, uint8_t options) const override
	{
		return std::make_shared<PatternClass>(creator, pattern, options);
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
	 * @param mod The module which caused this exception to be thrown.
	 * @param regex A regular expression which failed to compile.
	 * @param error The error which occurred whilst compiling the regular expression.
	*/
	Exception(const Module* mod, const std::string& regex, const std::string& error)
		: ModuleException(mod, FMT::format("Error in regex '{}': {}", regex, error))
	{
	}

	/** Initializes a new instance of the Regex::Exception class.
	 * @param mod The module which caused this exception to be thrown.
	 * @param regex A regular expression which failed to compile.
	 * @param error The error which occurred whilst compiling the regular expression.
	 * @param offset The offset at which the errror occurred.
	*/
	Exception(const Module* mod, const std::string& regex, const std::string& error, size_t offset)
		: ModuleException(mod, FMT::format("Error in regex '{}' at offset {}: {}", regex, offset, error))
	{
	}
};


class Regex::MatchCollection
{
private:
	/** The substrings that were captured. */
	const Captures captures;

	/** The substrings that were captured by name. */
	const NamedCaptures namedcaptures;

public:
	/** Initializes a new instance of the Regex::MatchCollection class.
	 * @param c The substrings that were captured.
	 * @param nc The substrings that were captured by name.
	 */
	MatchCollection(const Captures& c, const NamedCaptures& nc)
		: captures(c)
		, namedcaptures(nc)
	{
	}

	/** Retrieves a specific captured substring by index.
	 * @param idx The index of the captured substring to return.
	 * The requested captured substring or std::nullopt if it does not exist.
	 */
	const std::optional<std::string> GetCapture(size_t idx) const
	{
		return captures.size() > idx ? std::nullopt :  std::make_optional(captures[idx]);
	}

	/** Retrieves the substrings that were captured. */
	const Captures& GetCaptures() const { return captures; }

	/** Retrieves a specific captured substring by index.
	 * @param name The name of the captured substring to return.
	 * The requested captured substring or std::nullopt if it does not exist.
	 */
	const std::optional<std::string> GetNamedCapture(const std::string& name) const
	{
		auto capture = namedcaptures.find(name);
		return capture == namedcaptures.end() ? std::nullopt : std::make_optional(capture->second);
	}

	/** Retrieves the substrings that were captured by name. */
	const NamedCaptures& GetNamedCaptures() const { return namedcaptures; }
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

	/** Attempts to extract this pattern's match groups from the specified text.
	 * @param text The text to extract match groups from..
	 * @return If the text matched the pattern then a match collection; otherwise, std::nullopt.
	 */
	virtual std::optional<MatchCollection> Matches(const std::string& text) = 0;
};

inline Regex::PatternPtr Regex::Engine::CreateHuman(const std::string& pattern) const
{
	if (pattern.empty() || pattern[0] != '/')
		return Create(pattern, OPT_NONE);

	size_t end = pattern.find_last_not_of("Ii");
	if (!end || end == std::string::npos || pattern[end] != '/')
		throw Exception(creator, pattern, "Regex patterns must be terminated with a '/'!");

	uint8_t options = Regex::OPT_NONE;
	for (const auto flag : insp::iterator_range(pattern.begin() + end + 1, pattern.end()))
	{
		switch (flag)
		{
			case 'I':
				options &= ~OPT_CASE_INSENSITIVE;
				break;

			case 'i':
				options |= OPT_CASE_INSENSITIVE;
				break;
		}
	}
	return Create(pattern.substr(1, end - 1), options);
}
