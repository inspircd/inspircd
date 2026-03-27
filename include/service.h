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

/** A structure defining something that a module can provide */
class CoreExport ServiceProvider
	: public Cullable
{
public:
	/** Module that created this service */
	const reference<Module> service_creator;

	/** Name of the service being provided */
	const std::string service_name;

	/** Type of service (must match object type) */
	const std::string service_type;

	ServiceProvider(Module* mod, const std::string& stype, const std::string& sname);

	/** Register this service in the appropriate registrar. */
	virtual void RegisterService();

	/** Unregister this service in the appropriate registrar. */
	virtual void UnregisterService();

	/** If called, this ServiceProvider won't be registered automatically
	 */
	void DisableAutoRegister();

	/** Retrieves the name of the service creator. */
	std::string GetSource() const;
};

class CoreExport DataProvider
	: public ServiceProvider
{
public:
	DataProvider(Module* mod, const std::string& stype, const std::string& sname = "");

	/** @copydoc ServiceProvider::RegisterService */
	void RegisterService() override;

	/** @copydoc ServiceProvider::UnregisterService */
	void UnregisterService() override;
};
