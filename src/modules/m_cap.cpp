/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
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
#include "modules/reload.h"
#include "modules/cap.h"

namespace Cap
{
	class ManagerImpl;
}

static Cap::ManagerImpl* managerimpl;

class Cap::ManagerImpl : public Cap::Manager, public ReloadModule::EventListener
{
	/** Stores the cap state of a module being reloaded
	 */
	struct CapModData
	{
		struct Data
		{
			std::string name;
			std::vector<std::string> users;

			Data(Capability* cap)
				: name(cap->GetName())
			{
			}
		};
		std::vector<Data> caps;
	};

	typedef insp::flat_map<std::string, Capability*, irc::insensitive_swo> CapMap;

	ExtItem capext;
	CapMap caps;
	Events::ModuleEventProvider& evprov;

	static bool CanRequest(LocalUser* user, Ext usercaps, Capability* cap, bool adding)
	{
		const bool hascap = ((usercaps & cap->GetMask()) != 0);
		if (hascap == adding)
			return true;

		return cap->OnRequest(user, adding);
	}

	Capability::Bit AllocateBit() const
	{
		Capability::Bit used = 0;
		for (CapMap::const_iterator i = caps.begin(); i != caps.end(); ++i)
		{
			Capability* cap = i->second;
			used |= cap->GetMask();
		}

		for (unsigned int i = 0; i < MAX_CAPS; i++)
		{
			Capability::Bit bit = (1 << i);
			if (!(used & bit))
				return bit;
		}
		throw ModuleException("Too many caps");
	}

	void OnReloadModuleSave(Module* mod, ReloadModule::CustomData& cd) CXX11_OVERRIDE
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "OnReloadModuleSave()");
		if (mod == creator)
			return;

		CapModData* capmoddata = new CapModData;
		cd.add(this, capmoddata);

		for (CapMap::iterator i = caps.begin(); i != caps.end(); ++i)
		{
			Capability* cap = i->second;
			// Only save users of caps that belong to the module being reloaded
			if (cap->creator != mod)
				continue;

			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Module being reloaded implements cap %s, saving cap users", cap->GetName().c_str());
			capmoddata->caps.push_back(CapModData::Data(cap));
			CapModData::Data& capdata = capmoddata->caps.back();

			// Populate list with uuids of users who are using the cap
			const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
			for (UserManager::LocalList::const_iterator j = list.begin(); j != list.end(); ++j)
			{
				LocalUser* user = *j;
				if (cap->get(user))
					capdata.users.push_back(user->uuid);
			}
		}
	}

	void OnReloadModuleRestore(Module* mod, void* data) CXX11_OVERRIDE
	{
		CapModData* capmoddata = static_cast<CapModData*>(data);
		for (std::vector<CapModData::Data>::const_iterator i = capmoddata->caps.begin(); i != capmoddata->caps.end(); ++i)
		{
			const CapModData::Data& capdata = *i;
			Capability* cap = ManagerImpl::Find(capdata.name);
			if (!cap)
			{
				ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Cap %s is no longer available after reload", capdata.name.c_str());
				continue;
			}

			// Set back the cap for all users who were using it before the reload
			for (std::vector<std::string>::const_iterator j = capdata.users.begin(); j != capdata.users.end(); ++j)
			{
				const std::string& uuid = *j;
				User* user = ServerInstance->FindUUID(uuid);
				if (!user)
				{
					ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "User %s is gone when trying to restore cap %s", uuid.c_str(), capdata.name.c_str());
					continue;
				}

				cap->set(user, true);
			}
		}
		delete capmoddata;
	}

 public:
	ManagerImpl(Module* mod, Events::ModuleEventProvider& evprovref)
		: Cap::Manager(mod)
		, ReloadModule::EventListener(mod)
		, capext(mod)
		, evprov(evprovref)
	{
		managerimpl = this;
	}

	~ManagerImpl()
	{
		for (CapMap::iterator i = caps.begin(); i != caps.end(); ++i)
		{
			Capability* cap = i->second;
			cap->Unregister();
		}
	}

	void AddCap(Cap::Capability* cap) CXX11_OVERRIDE
	{
		// No-op if the cap is already registered.
		// This allows modules to call SetActive() on a cap without checking if it's active first.
		if (cap->IsRegistered())
			return;

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Registering cap %s", cap->GetName().c_str());
		cap->bit = AllocateBit();
		cap->extitem = &capext;
		caps.insert(std::make_pair(cap->GetName(), cap));
		ServerInstance->Modules.AddReferent("cap/" + cap->GetName(), cap);

		FOREACH_MOD_CUSTOM(evprov, Cap::EventListener, OnCapAddDel, (cap, true));
	}

	void DelCap(Cap::Capability* cap) CXX11_OVERRIDE
	{
		// No-op if the cap is not registered, see AddCap() above
		if (!cap->IsRegistered())
			return;

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Unregistering cap %s", cap->GetName().c_str());

		// Fire the event first so modules can still see who is using the cap which is being unregistered
		FOREACH_MOD_CUSTOM(evprov, Cap::EventListener, OnCapAddDel, (cap, false));

		// Turn off the cap for all users
		const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
		for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); ++i)
		{
			LocalUser* user = *i;
			cap->set(user, false);
		}

		ServerInstance->Modules.DelReferent(cap);
		cap->Unregister();
		caps.erase(cap->GetName());
	}

	Capability* Find(const std::string& capname) const CXX11_OVERRIDE
	{
		CapMap::const_iterator it = caps.find(capname);
		if (it != caps.end())
			return it->second;
		return NULL;
	}

	void NotifyValueChange(Capability* cap) CXX11_OVERRIDE
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Cap %s changed value", cap->GetName().c_str());
		FOREACH_MOD_CUSTOM(evprov, Cap::EventListener, OnCapValueChange, (cap));
	}

	Protocol GetProtocol(LocalUser* user) const
	{
		return ((capext.get(user) & CAP_302_BIT) ? CAP_302 : CAP_LEGACY);
	}

	void Set302Protocol(LocalUser* user)
	{
		capext.set(user, capext.get(user) | CAP_302_BIT);
	}

	bool HandleReq(LocalUser* user, const std::string& reqlist)
	{
		Ext usercaps = capext.get(user);
		irc::spacesepstream ss(reqlist);
		for (std::string capname; ss.GetToken(capname); )
		{
			bool remove = (capname[0] == '-');
			if (remove)
				capname.erase(capname.begin());

			Capability* cap = ManagerImpl::Find(capname);
			if ((!cap) || (!CanRequest(user, usercaps, cap, !remove)))
				return false;

			if (remove)
				usercaps = cap->DelFromMask(usercaps);
			else
				usercaps = cap->AddToMask(usercaps);
		}

		capext.set(user, usercaps);
		return true;
	}

	void HandleList(std::string& out, LocalUser* user, bool show_all, bool show_values, bool minus_prefix = false) const
	{
		Ext show_caps = (show_all ? ~0 : capext.get(user));

		for (CapMap::const_iterator i = caps.begin(); i != caps.end(); ++i)
		{
			Capability* cap = i->second;
			if (!(show_caps & cap->GetMask()))
				continue;

			if ((show_all) && (!cap->OnList(user)))
				continue;

			if (minus_prefix)
				out.push_back('-');
			out.append(cap->GetName());

			if (show_values)
			{
				const std::string* capvalue = cap->GetValue(user);
				if ((capvalue) && (!capvalue->empty()) && (capvalue->find(' ') == std::string::npos))
				{
					out.push_back('=');
					out.append(*capvalue, 0, MAX_VALUE_LENGTH);
				}
			}
			out.push_back(' ');
		}
	}

	void HandleClear(LocalUser* user, std::string& result)
	{
		HandleList(result, user, false, false, true);
		capext.unset(user);
	}
};

Cap::ExtItem::ExtItem(Module* mod)
	: LocalIntExt("caps", ExtensionItem::EXT_USER, mod)
{
}

std::string Cap::ExtItem::serialize(SerializeFormat format, const Extensible* container, void* item) const
{
	std::string ret;
	// XXX: Cast away the const because IS_LOCAL() doesn't handle it
	LocalUser* user = IS_LOCAL(const_cast<User*>(static_cast<const User*>(container)));
	if ((format == FORMAT_NETWORK) || (!user))
		return ret;

	// List requested caps
	managerimpl->HandleList(ret, user, false, false);

	// Serialize cap protocol version. If building a human-readable string append a new token, otherwise append only a single character indicating the version.
	Protocol protocol = managerimpl->GetProtocol(user);
	if (format == FORMAT_USER)
		ret.append("capversion=3.");
	else if (!ret.empty())
		ret.erase(ret.length()-1);

	if (protocol == CAP_302)
		ret.push_back('2');
	else
		ret.push_back('1');

	return ret;
}

void Cap::ExtItem::unserialize(SerializeFormat format, Extensible* container, const std::string& value)
{
	if (format == FORMAT_NETWORK)
		return;

	LocalUser* user = IS_LOCAL(static_cast<User*>(container));
	if (!user)
		return; // Can't happen

	// Process the cap protocol version which is a single character at the end of the serialized string
	const char verchar = *value.rbegin();
	if (verchar == '2')
		managerimpl->Set302Protocol(user);

	// Remove the version indicator from the string passed to HandleReq
	std::string caplist(value, 0, value.size()-1);
	managerimpl->HandleReq(user, caplist);
}

class CommandCap : public SplitCommand
{
	Events::ModuleEventProvider evprov;
	Cap::ManagerImpl manager;

	static void DisplayResult(LocalUser* user, std::string& result)
	{
		if (*result.rbegin() == ' ')
			result.erase(result.end()-1);
		user->WriteCommand("CAP", result);
	}

 public:
	LocalIntExt holdext;

	CommandCap(Module* mod)
		: SplitCommand(mod, "CAP", 1)
		, evprov(mod, "event/cap")
		, manager(mod, evprov)
		, holdext("cap_hold", ExtensionItem::EXT_USER, mod)
	{
		works_before_reg = true;
	}

	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser* user) CXX11_OVERRIDE
	{
		if (user->registered != REG_ALL)
			holdext.set(user, 1);

		std::string subcommand(parameters[0].length(), ' ');
		std::transform(parameters[0].begin(), parameters[0].end(), subcommand.begin(), ::toupper);

		if (subcommand == "REQ")
		{
			if (parameters.size() < 2)
				return CMD_FAILURE;

			std::string result = (manager.HandleReq(user, parameters[1]) ? "ACK :" : "NAK :");
			result.append(parameters[1]);
			user->WriteCommand("CAP", result);
		}
		else if (subcommand == "END")
		{
			holdext.unset(user);
		}
		else if ((subcommand == "LS") || (subcommand == "LIST"))
		{
			const bool is_ls = (subcommand.length() == 2);
			if ((is_ls) && (parameters.size() > 1) && (parameters[1] == "302"))
				manager.Set302Protocol(user);

			std::string result = subcommand + " :";
			// Show values only if supports v3.2 and doing LS
			manager.HandleList(result, user, is_ls, ((is_ls) && (manager.GetProtocol(user) != Cap::CAP_LEGACY)));
			DisplayResult(user, result);
		}
		else if ((subcommand == "CLEAR") && (manager.GetProtocol(user) == Cap::CAP_LEGACY))
		{
			std::string result = "ACK :";
			manager.HandleClear(user, result);
			DisplayResult(user, result);
		}
		else
		{
			user->WriteNumeric(ERR_INVALIDCAPSUBCOMMAND, subcommand, "Invalid CAP subcommand");
			return CMD_FAILURE;
		}

		return CMD_SUCCESS;
	}
};

class ModuleCap : public Module
{
	CommandCap cmd;

 public:
	ModuleCap()
		: cmd(this)
	{
	}

	ModResult OnCheckReady(LocalUser* user) CXX11_OVERRIDE
	{
		return (cmd.holdext.get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for CAP capability negotiation", VF_VENDOR);
	}
};

MODULE_INIT(ModuleCap)
