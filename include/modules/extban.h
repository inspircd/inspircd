/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2024, 2026 Sadie Powell <sadie@witchery.services>
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

#include <listmode.h>

#ifndef INSPIRCD_EXTBAN
# define INSPIRCD_EXTBAN
#endif

namespace ExtBan
{
	class Acting;
	class ActingBase;
	class Base;
	class EventListener;
	class MatchConfig;
	class MatchingBase;
	class Manager;
	class ManagerRef;

	/** All possible results for CompareResult. */
	enum class Comparison
		: uint8_t
	{
		/** The value passed to CompareResult is not an extban. */
		NOT_AN_EXTBAN,

		/** The extban is equivalent to the specified value. */
		MATCH,

		/** The extban is not equivalent to the specified value. */
		NOT_MATCH,
	};

	/** All possible extban formats. */
	enum class Format
		: uint8_t
	{
		/** Do not perform any normalisation of extbans. */
		ANY,

		/** Normalise extbans to use their name (e.g. mute). */
		NAME,

		/** Normalise extbans to use their letter (e.g. m). */
		LETTER,
	};

	enum MatchFlags
		: uint8_t
	{
		/** There are no special rules for matching. */
		MATCH_DEFAULT = 0,

		/** The channel parameter to IsMatch can not be nullptr. */
		MATCH_REQUIRE_CHANNEL = 1,
	};

	/** All possible types of extban. */
	enum class Type
		: uint8_t
	{
		/** The extban takes action against specific behaviour (e.g. nokicks). */
		ACTING,

		/** The extban matches against a specific pattern (e.g. sslfp). */
		MATCHING
	};

	/** The underlying type of an extban letter. */
	using Letter = std::string::value_type;

	/** Parses a ban entry and extracts an extban from it.
	 * @param banentry The ban entry to parse.
	 * @param name The parsed name of the extban.
	 * @param value The parsed value of the extban.
	 * @param inverted Whether the extban has been inverted.
	 * @return True if an extban was extracted from the ban entry; otherwise, false.
	 */
	inline bool Parse(const std::string& banentry, std::string& name, std::string& value, bool& inverted);
}

class ExtBan::MatchConfig final
{
public:
	using MatchFn = std::function<bool(ListModeBase*, User*, Channel*, const std::string&, const ExtBan::MatchConfig&)>;

	/** Whether to match against the real mask. */
	bool match_real_mask = ServerInstance->Config->BanRealMask;

	/** The function to use when performing the next match. */
	MatchFn next_match = [](auto* lm, auto* user, auto* chan, const auto& text, const auto& config)
	{
		return chan->CheckListEntry(lm, user, text, config.match_real_mask);
	};
};

/** Manager for the extban system. */
class ExtBan::Manager
	: public Service::SimpleProvider
{
protected:
	/** Initializes an instance of the ExtBan::Base class.
	 * @param mod The module which created this instance.
	 */
	Manager(const WeakModulePtr& mod)
		: Service::SimpleProvider(mod, "extbanmanager")
	{
	}

public:
	/** A mapping of extban letters to their associated object. */
	using LetterMap = std::unordered_map<ExtBan::Letter, ExtBan::Base*>;

	/** A mapping of extban names to their associated objects. */
	using NameMap = insp::casemapped_unordered_map<ExtBan::Base*>;

	/** Registers an extban with the manager.
	 * @param extban The extban instance to register.
	 */
	virtual void AddExtBan(Base* extban) = 0;

	/** Canonicalises a list mode entry if it is an extban.
	 * @param text The list mode entry to canonicalize.
	 * @return True if the text was a valid extban and was canonicalised. Otherwise, false.
	 */
	virtual bool Canonicalize(std::string& text) const = 0;

	/** Compares an entry from this list with the specified value.
	 * @param lm The list mode which is the entry exists on.
	 * @param entry The list entry to compare against.
	 * @param value The value to compare to.
	 * @return MATCH if the entries match and NOT_MATCH if the entries do not match.
	 */
	virtual Comparison CompareEntry(const ListModeBase* lm, const std::string& entry, const std::string& value) const = 0;

	/** Unregisters an extban from the manager.
	 * @param extban The extban instance to unregister.
	 */
	virtual void DelExtBan(Base* extban) = 0;

	/** Retrieves the method used for normalising extbans. */
	virtual Format GetFormat() const = 0;

	/** Retrieves a mapping of extban letters to their associated object. */
	virtual const LetterMap& GetLetterMap() const = 0;

	/** Retrieves a mapping of extban names to their associated object. */
	virtual const NameMap& GetNameMap() const = 0;

	/** Retrieves the status of an acting extban.
	 * @param extban The extban to get the status of.
	 * @param user The user to match the extban against.
	 * @param channel The channel which the extban is set on.
	 * @param config The configuration to use when matching against the user.
	 * @return MOD_RES_ALLOW if the user is exempted, MOD_RES_DENY if the user is banned, or
	 *         MOD_RES_PASSTHRU if the extban is not set.
	 */
	virtual ModResult GetStatus(ActingBase* extban, User* user, Channel* channel, const std::optional<MatchConfig>& config = std::nullopt) const = 0;

	/** Finds an extban by name or letter.
	 * @param xbname The name or letter of the extban to find.
	 */
	Base* Find(const std::string& xbname) const { return xbname.length() == 1 ? FindLetter(xbname[0]) : FindName(xbname); }

	/** Finds an extban by letter.
	 * @param xbletter The letter of the extban to find.
	 */
	virtual Base* FindLetter(ExtBan::Letter xbletter) const = 0;

	/** Finds an extban by name.
	 * @param xbname The name of the extban to find.
	 */
	virtual Base* FindName(const std::string& xbname) const = 0;

	/** Validates an extban.
	 * @param lm The mode for which the extban is being added.
	 * @param user The user who is adding the extban
	 * @param channel The channel the extban is being added mon.
	 * @param text The text of the extban to validate.
	 * @return MATCH if the extban is valid, NOT_MATCH if the extban is not valid, and NOT_AN_EXTBAN
	 * *       if the text is not an extban.
	 */
	virtual Comparison Validate(ListModeBase* lm, LocalUser* user, Channel* channel, std::string& text) const = 0;
};

/** Dynamic reference to the extban manager class. */
class ExtBan::ManagerRef final
	: public dynamic_reference_nocheck<ExtBan::Manager>
{
public:
	ManagerRef(const WeakModulePtr& mod)
		: dynamic_reference_nocheck<ExtBan::Manager>(mod, "extbanmanager")
	{
	}
};

/** Base class for types of extban. */
class ExtBan::Base
	: public Service::Provider
	, private dynamic_reference_base::CaptureHook
{
private:
	/** Whether this ExtBan is currently enabled. */
	bool active = false;

	/** The character used in bans to signify this extban (e.g. z). */
	ExtBan::Letter letter;

	/** A reference to the extban manager. */
	dynamic_reference<Manager> manager;

	/** The flags used for matching. */
	uint16_t match_flags;

	/** @copydoc dynamic_reference_base::CaptureHook::OnCapture */
	void OnCapture() override
	{
		if (active)
			SetActive(true);
	}

protected:
	/** Initializes an instance of the ExtBan::Base class.
	 * @param mod The module which created this instance.
	 * @param xbname The name used in bans to signify this extban.
	 * @param xbletter The character used in bans to signify this extban.
	 * @param xbmatchflags The flags used for matching.
	 */
	Base(const WeakModulePtr& mod, const std::string& xbname, ExtBan::Letter xbletter, uint8_t xbmatchflags = ExtBan::MATCH_DEFAULT)
		: Service::Provider(mod, "ExtBan::Base", xbname)
		, letter(ServerInstance->Config->ConfValue("extbans")->getCharacter(xbname, xbletter, true))
		, manager(mod, "extbanmanager")
		, match_flags(xbmatchflags)
	{
	}

	~Base() override
	{
		SetActive(false);
	}

public:
	/** Canonicalises a value for this extban.
	 * @param text The value to canonicalize.
	 */
	virtual void Canonicalize(std::string& text) { }

	/** Validates an extban
	 * @param lm The mode for which the extban is being added.
	 * @param user The user who is adding the extban
	 * @param channel The channel the extban is being added mon.
	 * @param text The text of the extban to validate.
	 * @return True if the extban is valid; otherwise, false.
	 */
	virtual bool Validate(ListModeBase* lm, LocalUser* user, Channel* channel, std::string& text)
	{
		Canonicalize(text);
		return true;
	}

	/** Retrieves the character used in bans to signify this extban. */
	ExtBan::Letter GetLetter() const { return letter; }

	/** Retrieves a pointer to the extban manager. */
	Manager* GetManager() { return manager ? *manager : nullptr; }

	/** Retrieves the flags used for matching. */
	auto GetMatchFlags() const { return match_flags; }

	/** Retrieves the name used in bans to signify this extban. */
	const std::string& GetName() const { return service_name; }

	/** Retrieves the type of this extban. */
	virtual Type GetType() const = 0;

	/** Retrieves whether this extban is enabled. */
	bool IsActive() const { return active; }

	/** Determines whether the specified user matches this extban.
	 * @param lm The list mode which the extban is set on.
	 * @param user The user to match the text against.
	 * @param channel The channel which the extban is set on.
	 * @param text The string to match the user against.
	 * @param config The configuration to use when matching against the user.
	 * @return True if the user matches the extban; otherwise, false.
	 */
	virtual bool IsMatch(ListModeBase* lm, User* user, Channel* channel, const std::string& text, const ExtBan::MatchConfig& config) = 0;

	/** @copydoc Service::Provider::RegisterService */
	void RegisterService() override
	{
		manager.SetCaptureHook(this);
		SetActive(true);
	}

	/** @copydoc Service::Provider::UnregisterService */
	void UnregisterService() override
	{
		manager.SetCaptureHook(nullptr);
		SetActive(false);
	}

	/** Toggles the active status of this extban.
	 * @param Active Whether this extban is active or not.
	 */
	void SetActive(bool Active)
	{
		active = Active;
		if (manager)
		{
			if (active)
				manager->AddExtBan(this);
			else
				manager->DelExtBan(this);
		}
	}
};

/** Base class for acting extbans. */
class ExtBan::ActingBase
	: public Base
{
protected:
	/** Initializes an instance of the ExtBan::ActingBase class.
	 * @param mod The module which created this instance.
	 * @param xbname The name used in bans to signify this extban.
	 * @param xbletter The character used in bans to signify this extban.
	 * @param xbmatchflags The flags used for matching.
	 */
	ActingBase(const WeakModulePtr& mod, const std::string& xbname, ExtBan::Letter xbletter, uint8_t xbmatchflags = ExtBan::MATCH_DEFAULT)
		: Base(mod, xbname, xbletter, xbmatchflags | ExtBan::MATCH_REQUIRE_CHANNEL)
	{
	}

public:
	/** @copydoc ExtBan::Base::Canonicalize */
	void Canonicalize(std::string& text) override
	{
		if (!GetManager() || !GetManager()->Canonicalize(text))
			ModeParser::CleanMask(text);
	}

	/** @copydoc ExtBan::Base::Validate */
	bool Validate(ListModeBase* lm, LocalUser* user, Channel* channel, std::string& text) override
	{
		if (GetManager())
		{
			const auto valid = GetManager()->Validate(lm, user, channel, text);
			if (valid != ExtBan::Comparison::NOT_AN_EXTBAN)
				return valid == ExtBan::Comparison::MATCH;
		}
		ModeParser::CleanMask(text);
		return true;
	}

	/** @copydoc ExtBan::Base::GetType */
	Type GetType() const override { return ExtBan::Type::ACTING; }

	/** @copydoc ExtBan::Base::IsMatch */
	bool IsMatch(ListModeBase* lm, User* user, Channel* channel, const std::string& text, const ExtBan::MatchConfig& config) override
	{
		return config.next_match(lm, user, channel, text, config);
	}
};

/** A simple acting extban that has no fields. */
class ExtBan::Acting
	: public ActingBase
{
public:
	/** Initializes an instance of the ExtBan::Acting class.
	 * @param mod The module which created this instance.
	 * @param xbname The name used in bans to signify this extban.
	 * @param xbletter The character used in bans to signify this extban.
	 */
	Acting(const WeakModulePtr& mod, const std::string& xbname, ExtBan::Letter xbletter)
		: ActingBase(mod, xbname, xbletter)
	{
	}

	/** Determines whether the specified user matches this acting extban on the specified channel.
	 * @param user The user to check.
	 * @param channel The channel to check on.
	 * @param config The configuration to use when matching against the user.
	 * @return MOD_RES_ALLOW to explicitly allow their action, MOD_RES_DENY to expicitly deny their
	 *         action, or MOD_RES_PASSTHRU to let the default behaviour apply.
	 */
	ModResult GetStatus(User* user, Channel* channel, const std::optional<ExtBan::MatchConfig>& config = std::nullopt)
	{
		if (!GetManager())
			return MOD_RES_PASSTHRU;

		return GetManager()->GetStatus(this, user, channel, config);
	}
};

/** Base class for matching extbans. */
class ExtBan::MatchingBase
	: public Base
{
protected:
	/** Initializes an instance of the ExtBan::MatchingBase class.
	 * @param mod The module which created this instance.
	 * @param xbname The name used in bans to signify this extban.
	 * @param xbletter The character used in bans to signify this extban.
	 * @param xbmatchflags The flags used for matching.
	 */
	MatchingBase(const WeakModulePtr& mod, const std::string& xbname, ExtBan::Letter xbletter, uint8_t xbmatchflags = ExtBan::MATCH_DEFAULT)
		: Base(mod, xbname, xbletter, xbmatchflags)
	{
	}

public:
	/** @copydoc ExtBan::Base::GetType */
	Type GetType() const override { return ExtBan::Type::MATCHING; }

	/** @copydoc ExtBan::Base::IsMatch */
	virtual bool IsMatch(ListModeBase* lm, User* user, Channel* channel, const std::string& text, const ExtBan::MatchConfig& config) override = 0;
};

/** Provides events relating to extbans. */
class ExtBan::EventListener
	: public Events::ModuleEventListener
{
protected:
	EventListener(const WeakModulePtr& mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "extban", eventprio)
	{
	}

public:
	/** Called when an extban is being checked.
	 * @param user The user which the extban is being checked against.
	 * @param chan The channel which the extban is set on.
	 * @param extban The extban which is being checked against.
	 * @param config The configuration to use when matching against the user.
	 */
	virtual ModResult OnExtBanCheck(User* user, Channel* chan, ExtBan::ActingBase* extban, const ExtBan::MatchConfig& config) = 0;
};

inline bool ExtBan::Parse(const std::string& banentry, std::string& name, std::string& value, bool& inverted)
{
	// The mask must be in the format [!]<letter>:<value> or [!]<name>:<value>.
	inverted = false;
	size_t startpos = 0;
	if (banentry[0] == '!')
	{
		inverted = true;
		startpos++;
	}

	size_t endpos = banentry.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", startpos);
	if (endpos == std::string::npos || banentry[endpos] != ':')
		return false;

	name.assign(banentry, startpos, endpos - startpos);
	value.assign(banentry, endpos + 1);
	return true;
}
