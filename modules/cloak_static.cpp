/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2023, 2025 Sadie Powell <sadie@witchery.services>
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

class StaticMethod final
	: public Cloak::Method
{
private:
	// The cloak to set on users.
	std::string cloak;

public:
	StaticMethod(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, const std::string& c) ATTR_NOT_NULL(2)
		: Cloak::Method(engine, tag)
		, cloak(c)
	{
	}

	std::optional<Cloak::Info> Cloak(LocalUser* user) override ATTR_NOT_NULL(2)
	{
		if (!MatchesUser(user))
			return std::nullopt; // We shouldn't cloak this user.

		return cloak;
	}

	std::optional<Cloak::Info> Cloak(const std::string& hostip) override
	{
		return cloak;
	}

	void GetLinkData(Module::LinkData& data) override
	{
		data["cloak"] = cloak;
	}
};

class StaticEngine final
	: public Cloak::Engine
{
public:
	StaticEngine(Module* Creator)
		: Cloak::Engine(Creator, "static")
	{
	}

	Cloak::MethodPtr Create(const std::shared_ptr<ConfigTag>& tag, bool primary) override
	{
		const std::string cloak = tag->getString("cloak");
		if (cloak.empty() || cloak.length() > ServerInstance->Config->Limits.MaxHost)
		{
			throw ModuleException(creator, "Your static cloak must be between 1 and {} characters long, at {}",
				ServerInstance->Config->Limits.MaxHost, tag->source.str());
		}

		return std::make_shared<StaticMethod>(this, tag, cloak);
	}
};

class ModuleCloakStatic final
	: public Module
{
private:
	StaticEngine nickcloak;

public:
	ModuleCloakStatic()
		: Module(VF_VENDOR, "Adds the static cloaking method for use with the cloak module.")
		, nickcloak(this)
	{
	}
};

MODULE_INIT(ModuleCloakStatic)
