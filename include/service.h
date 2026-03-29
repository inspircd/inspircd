/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 Sadie Powell <sadie@witchery.services>
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

namespace Service
{
	struct PairCompare;
	class Provider;
	class SimpleProvider;

	/** Holds one or more services. */
	using List = std::vector<Service::Provider*>;

	/** Holds a service type and names. */
	using Pair = std::pair<std::string, std::string>;
}

/** Compares two service pairs case insensitively using the IRC casemapping. */
struct Service::PairCompare final
{
	bool operator()(const Service::Pair& lhs, const Service::Pair& rhs) const
	{
		if (!insp::casemapped_equals(lhs.first, rhs.first))
			return insp::casemapped_less(lhs.first, rhs.first);
		return insp::casemapped_less(lhs.second, rhs.second);
	}
};

/** A structure defining something that a module can provide */
class CoreExport Service::Provider
	: public Cullable
{
public:
	/** Module that created this service */
	const WeakModulePtr service_creator;

	/** Name of the service being provided */
	const std::string service_name;

	/** Type of service (must match object type) */
	const std::string service_type;

	/** Creates a new instance of the Provider class.
	 * @param mod The module which created this instance.
	 * @param stype The type of the service (e.g. CommandBase).
	 * @param sname The name of the service (e.g. PRIVMSG).
	 */
	Provider(const WeakModulePtr& mod, const std::string& stype, const std::string& sname);

	/** Register this service in the appropriate registrar. */
	virtual void RegisterService();

	/** Unregister this service in the appropriate registrar. */
	virtual void UnregisterService();

	/** If called, this service won't be registered automatically. */
	void DisableAutoRegister();

	/** Retrieves the name of the service creator. */
	std::string GetSource() const;
};

class CoreExport Service::SimpleProvider
	: public Service::Provider
{
public:
	/** Creates a new instance of the SimpleProvider class.
	 * @param mod The module which created this instance.
	 * @param stype The type of the service (e.g. CommandBase).
	 * @param sname The name of the service (e.g. PRIVMSG).
	 */
	SimpleProvider(const WeakModulePtr& mod, const std::string& stype, const std::string& sname = "");

	/** @copydoc Service::Provider::RegisterService */
	void RegisterService() override;

	/** @copydoc Service::Provider::UnregisterService */
	void UnregisterService() override;
};
