/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2022, 2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015, 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "modules/cap.h"
#include "modules/reload.h"

class CapNotify final
	: public Cap::Capability
{
	bool OnRequest(LocalUser* user, bool add) override
	{
		// Users using the negotiation protocol v3.2 or newer may not turn off cap-notify
		return add || GetProtocol(user) == Cap::CAP_LEGACY;
	}

	bool OnList(LocalUser* user) override
	{
		// If the client supports 3.2 enable cap-notify for them
		if (GetProtocol(user) != Cap::CAP_LEGACY)
			Set(user, true);
		return true;
	}

public:
	CapNotify(Module* mod)
		: Cap::Capability(mod, "cap-notify")
	{
	}
};

class CapNotifyMessage final
	: public Cap::MessageBase
{
public:
	CapNotifyMessage(bool add, const std::string& capname)
		: Cap::MessageBase((add ? "NEW" : "DEL"))
	{
		PushParamRef(capname);
	}
};

class CapNotifyValueMessage final
	: public Cap::MessageBase
{
	std::string s;
	const std::string::size_type pos;

public:
	CapNotifyValueMessage(const std::string& capname)
		: Cap::MessageBase("NEW")
		, s(capname)
		, pos(s.size()+1)
	{
		s.push_back('=');
		PushParamRef(s);
	}

	void SetCapValue(const std::string& capvalue)
	{
		s.erase(pos);
		s.append(capvalue);
		InvalidateCache();
	}
};

class ModuleIRCv3CapNotify final
	: public Module
	, public Cap::EventListener
	, public ReloadModule::EventListener
{
	CapNotify capnotify;
	std::string reloadedmod;
	std::vector<std::string> reloadedcaps;
	ClientProtocol::EventProvider protoev;

	void Send(const std::string& capname, Cap::Capability* cap, bool add)
	{
		CapNotifyMessage msg(add, capname);
		CapNotifyValueMessage msgwithval(capname);

		ClientProtocol::Event event(protoev, msg);
		ClientProtocol::Event eventwithval(protoev, msgwithval);

		for (auto* user : ServerInstance->Users.GetLocalUsers())
		{
			if (!capnotify.IsEnabled(user))
				continue;

			// Check that this user can actually see the cap.
			if (add && (!cap || !cap->OnList(user)))
				continue;

			// If the cap is being added and the client supports cap values then show the value, if any
			if ((add) && (capnotify.GetProtocol(user) != Cap::CAP_LEGACY))
			{
				const std::string* capvalue = cap->GetValue(user);
				if ((capvalue) && (!capvalue->empty()))
				{
					msgwithval.SetUser(user);
					msgwithval.SetCapValue(*capvalue);
					user->Send(eventwithval);
					continue;
				}
			}
			msg.SetUser(user);
			user->Send(event);
		}
	}

public:
	ModuleIRCv3CapNotify()
		: Module(VF_VENDOR, "Provides the IRCv3 cap-notify client capability.")
		, Cap::EventListener(this)
		, ReloadModule::EventListener(this)
		, capnotify(this)
		, protoev(this, "CAP_NOTIFY")
	{
	}

	void OnCapAddDel(Cap::Capability* cap, bool add) override
	{
		if (cap->creator == this)
			return;

		if (cap->creator->ModuleFile == reloadedmod)
		{
			if (!add)
				reloadedcaps.push_back(cap->GetName());
			return;
		}
		Send(cap->GetName(), cap, add);
	}

	void OnCapValueChange(Cap::Capability* cap) override
	{
		// The value of a cap has changed, send CAP DEL and CAP NEW with the new value
		Send(cap->GetName(), cap, false);
		Send(cap->GetName(), cap, true);
	}

	void OnReloadModuleSave(Module* mod, ReloadModule::CustomData& cd) override
	{
		if (mod == this)
			return;
		reloadedmod = mod->ModuleFile;
		// Request callback when reload is complete
		cd.add(this, nullptr);
	}

	void OnReloadModuleRestore(Module* mod, void* data) override
	{
		// Reloading can change the set of caps provided by a module so assuming that if the reload succeeded all
		// caps that the module previously provided are available or all were lost if the reload failed is wrong.
		// Instead, we verify the availability of each cap individually.
		dynamic_reference_nocheck<Cap::Manager> capmanager(this, "capmanager");
		if (capmanager)
		{
			for (const auto& capname : reloadedcaps)
			{
				if (!capmanager->Find(capname))
					Send(capname, nullptr, false);
			}
		}
		reloadedmod.clear();
		reloadedcaps.clear();
	}
};

MODULE_INIT(ModuleIRCv3CapNotify)
