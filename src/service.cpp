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

static insp::intrusive_list<dynamic_reference_base>* dynrefs = nullptr;

dynamic_reference_base::dynamic_reference_base(const WeakModulePtr& mod, const std::string& stype, const std::string& sname, bool strict)
	: service_name(sname)
	, service_type(stype)
	, strict_ref(strict)
	, creator(mod)
{
	if (!dynrefs)
		dynrefs = new insp::intrusive_list<dynamic_reference_base>;
	dynrefs->push_front(this);

	// Resolve unless there is no ModuleManager (part of class InspIRCd)
	if (ServerInstance)
		resolve();
}

dynamic_reference_base::dynamic_reference_base(const dynamic_reference_base& other)
	: dynamic_reference_base(other.creator, other.service_type, other.service_name, other.strict_ref)
{
	// Intentionally left blank.
}

dynamic_reference_base::~dynamic_reference_base()
{
	dynrefs->erase(this);
	if (dynrefs->empty())
		insp::delete_zero(dynrefs);
}

void dynamic_reference_base::reset_all(const std::string& stype)
{
	if (!dynrefs)
		return;

	for (auto* dynref : *dynrefs)
	{
		if (stype.empty() || dynref->service_type == stype)
			dynref->resolve();
	}
}

void dynamic_reference_base::resolve()
{
	auto* oldvalue = this->value;
	this->value = nullptr;

	// Because find() may return any element with a matching key in case count(key) > 1 use lower_bound()
	// to ensure a dynref with the same name as another one resolves to the same object
	auto i = ServerInstance->Modules.Services.lower_bound(std::make_pair(this->service_type, this->service_name));
	if (i == ServerInstance->Modules.Services.end())
		return; // No service found.

	const auto& [stype, sname] = i->first;
	if (!insp::casemapped_equals(this->service_type, stype) || !insp::casemapped_equals(this->service_name, sname))
		return; // No service found.

	if (this->strict_ref && sname.empty())
		return; // Can't use generic services for strict references.

	this->value = i->second;
	if (oldvalue != value && hook)
		hook->OnCapture();
}

void dynamic_reference_base::ClearProviderName()
{
	this->service_name.clear();
	value = nullptr;
}

void dynamic_reference_base::SetProvider(const std::string& stype, const std::string& sname)
{
	this->service_type = stype;
	this->service_name = sname;
	resolve();
}

void dynamic_reference_base::SetProviderName(const std::string& sname)
{
	SetProvider(GetProviderType(), sname);
}

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
