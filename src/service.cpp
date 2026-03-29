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


#include "inspircd.h"

Service::Provider::Provider(const WeakModulePtr& mod, const std::string& stype, const std::string& sname)
	: service_creator(mod)
	, service_name(sname)
	, service_type(stype)
{
	if ((ServerInstance) && (ServerInstance->Modules.NewServices))
		ServerInstance->Modules.NewServices->push_back(this);
}

void Service::Provider::DisableAutoRegister()
{
	if ((ServerInstance) && (ServerInstance->Modules.NewServices))
		std::erase(*ServerInstance->Modules.NewServices, this);
}

std::string Service::Provider::GetSource() const
{
	if (insp::empty_ptr(this->service_creator))
		return "the core";

	const auto mod = this->service_creator.lock();
	if (!mod)
		return "an unknown module";

	return FMT::format("the {} module", ModuleManager::ShrinkModName(mod->ModuleFile));
}

void Service::Provider::RegisterService()
{
	// Intentionally left blank.
}

void Service::Provider::UnregisterService()
{
	// Intentionally left blank.
}

Service::SimpleProvider::SimpleProvider(const WeakModulePtr& mod, const std::string& stype, const std::string& sname)
	: Service::Provider(mod, stype, sname)
{
}

void Service::SimpleProvider::RegisterService()
{
	if (!this->service_name.empty())
		ServerInstance->Modules.AddReferent(this->service_type, this->service_name, this);
	ServerInstance->Modules.AddReferent(this->service_type, "", this);
}

void Service::SimpleProvider::UnregisterService()
{
	ServerInstance->Modules.DelReferent(this);
}
