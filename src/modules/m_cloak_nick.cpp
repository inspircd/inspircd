/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2023 Sadie Powell <sadie@witchery.services>
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
#include "modules/cloak.h"

class NickMethod final
	: public Cloak::Method
{
protected:
	// The characters which are valid in a hostname.
	const CharState& hostmap;

	// The prefix for cloaks (e.g. MyNet).
	const std::string prefix;

	// Whether to strip non-host characters from the cloak.
	const bool sanitize;

	// The suffix for IP cloaks (e.g. IP).
	const std::string suffix;

public:
	NickMethod(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, const CharState& hm) ATTR_NOT_NULL(2)
		: Cloak::Method(engine, tag)
		, hostmap(hm)
		, prefix(tag->getString("prefix"))
		, sanitize(tag->getBool("sanitize", true))
		, suffix(tag->getString("suffix"))
	{
	}

	std::string Generate(LocalUser* user) override ATTR_NOT_NULL(2)
	{
		if (!MatchesUser(user))
			return {}; // We shouldn't cloak this user.

		std::string safenick;
		safenick.reserve(user->nick.length());
		for (const auto chr : user->nick)
		{
			if (!hostmap.test(static_cast<unsigned char>(chr)))
			{
				if (!sanitize)
					return {}; // Contains invalid characters.

				continue;
			}

			safenick.push_back(chr);
		}

		if (safenick.empty())
			return {}; // No cloak.

		return prefix + safenick + suffix;
	}

	std::string Generate(const std::string& hostip) override
	{
		// We can't generate nick cloaks without a user.
		return {};
	}

	void GetLinkData(Module::LinkData& data, std::string& compatdata) override
	{
		data["prefix"]   = prefix;
		data["sanitize"] = sanitize ? "yes" : "no";
		data["suffix"]   = suffix;
	}
};

class NickEngine final
	: public Cloak::Engine
{
public:
	// The characters which are valid in a hostname.
	CharState hostmap;

	NickEngine(Module* Creator)
		: Cloak::Engine(Creator, "nick")
	{
	}

	Cloak::MethodPtr Create(const std::shared_ptr<ConfigTag>& tag, bool primary) override
	{
		return std::make_shared<NickMethod>(this, tag, hostmap);
	}
};

class ModuleCloakNick final
	: public Module
{
private:
	Cloak::API cloakapi;
	NickEngine nickcloak;

public:
	ModuleCloakNick()
		: Module(VF_VENDOR, "Adds the nick cloaking method for use with the cloak module.")
		, cloakapi(this)
		, nickcloak(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		CharState newhostmap;
		const auto& tag = ServerInstance->Config->ConfValue("hostname");
		for (const auto chr : tag->getString("charmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789", 1))
		{
			// A hostname can not contain NUL, LF, CR, or SPACE.
			if (chr == 0x00 || chr == 0x0A || chr == 0x0D || chr == 0x20)
				throw ModuleException(this, INSP_FORMAT("<hostname:charmap> can not contain character 0x{:02X} ({})", chr, chr));
			newhostmap.set(static_cast<unsigned char>(chr));
		}
		std::swap(newhostmap, nickcloak.hostmap);
	}

	void OnUserPostNick(User* user, const std::string& oldnick) override
	{
		LocalUser* luser = IS_LOCAL(user);
		if (luser && cloakapi && cloakapi->IsActiveCloak(nickcloak))
			cloakapi->ResetCloaks(luser, true);
	}
};

MODULE_INIT(ModuleCloakNick)
