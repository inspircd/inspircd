/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015-2016, 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "utility/string.h"

enum
{
	// From IRCv3 capability-negotiation-3.1.
	ERR_INVALIDCAPCMD = 410
};

namespace Cap
{
	class ManagerImpl;
}

static Cap::ManagerImpl* managerimpl;

class Cap::ManagerImpl final
	: public Cap::Manager
	, public ReloadModule::EventListener
{
	/** Stores the cap state of a module being reloaded
	 */
	struct CapModData final
	{
		struct Data final
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
		for (const auto& [_, cap] : caps)
			used |= cap->GetMask();

		for (size_t i = 0; i < MAX_CAPS; i++)
		{
			Capability::Bit bit = (static_cast<Capability::Bit>(1) << i);
			if (!(used & bit))
				return bit;
		}
		throw ModuleException(creator, "Too many caps");
	}

	void OnReloadModuleSave(Module* mod, ReloadModule::CustomData& cd) override
	{
		ServerInstance->Logs.Debug(MODNAME, "OnReloadModuleSave()");
		if (mod == creator)
			return;

		auto* capmoddata = new CapModData();
		cd.add(this, capmoddata);

		for (const auto& [_, cap] : caps)
		{
			// Only save users of caps that belong to the module being reloaded
			if (cap->creator != mod)
				continue;

			ServerInstance->Logs.Debug(MODNAME, "Module being reloaded implements cap {}, saving cap users", cap->GetName());
			capmoddata->caps.emplace_back(cap);
			CapModData::Data& capdata = capmoddata->caps.back();

			// Populate list with uuids of users who are using the cap
			for (auto* user : ServerInstance->Users.GetLocalUsers())
			{
				if (cap->IsEnabled(user))
					capdata.users.push_back(user->uuid);
			}
		}
	}

	void OnReloadModuleRestore(Module* mod, void* data) override
	{
		CapModData* capmoddata = static_cast<CapModData*>(data);
		for (const auto& capdata : capmoddata->caps)
		{
			Capability* cap = ManagerImpl::Find(capdata.name);
			if (!cap)
			{
				ServerInstance->Logs.Debug(MODNAME, "Cap {} is no longer available after reload", capdata.name);
				continue;
			}

			// Set back the cap for all users who were using it before the reload
			for (const auto& uuid : capdata.users)
			{
				auto* user = ServerInstance->Users.FindUUID(uuid);
				if (!user)
				{
					ServerInstance->Logs.Debug(MODNAME, "User {} is gone when trying to restore cap {}", uuid, capdata.name);
					continue;
				}

				cap->Set(user, true);
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

	~ManagerImpl() override
	{
		for (const auto& [_, cap] : caps)
			cap->Unregister();
	}

	void AddCap(Cap::Capability* cap) override
	{
		// No-op if the cap is already registered.
		// This allows modules to call SetActive() on a cap without checking if it's active first.
		if (cap->IsRegistered())
			return;

		ServerInstance->Logs.Debug(MODNAME, "Registering cap {}", cap->GetName());
		cap->bit = AllocateBit();
		cap->extitem = &capext;
		caps.emplace(cap->GetName(), cap);
		ServerInstance->Modules.AddReferent("cap/" + cap->GetName(), cap);

		evprov.Call(&Cap::EventListener::OnCapAddDel, cap, true);
	}

	void DelCap(Cap::Capability* cap) override
	{
		// No-op if the cap is not registered, see AddCap() above
		if (!cap->IsRegistered())
			return;

		ServerInstance->Logs.Debug(MODNAME, "Unregistering cap {}", cap->GetName());

		// Fire the event first so modules can still see who is using the cap which is being unregistered
		evprov.Call(&Cap::EventListener::OnCapAddDel, cap, false);

		// Turn off the cap for all users
		for (auto* user : ServerInstance->Users.GetLocalUsers())
			cap->Set(user, false);

		ServerInstance->Modules.DelReferent(cap);
		cap->Unregister();
		caps.erase(cap->GetName());
	}

	Capability* Find(const std::string& capname) const override
	{
		CapMap::const_iterator it = caps.find(capname);
		if (it != caps.end())
			return it->second;
		return nullptr;
	}

	void NotifyValueChange(Capability* cap) override
	{
		ServerInstance->Logs.Debug(MODNAME, "Cap {} changed value", cap->GetName());
		evprov.Call(&Cap::EventListener::OnCapValueChange, cap);
	}

	Protocol GetProtocol(LocalUser* user) const
	{
		return ((capext.Get(user) & CAP_302_BIT) ? CAP_302 : CAP_LEGACY);
	}

	void Set302Protocol(LocalUser* user)
	{
		capext.Set(user, capext.Get(user) | CAP_302_BIT);
	}

	bool HandleReq(LocalUser* user, const std::string& reqlist)
	{
		Ext usercaps = capext.Get(user);
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

		capext.Set(user, usercaps);
		return true;
	}

	void HandleList(std::vector<std::string>& out, LocalUser* user, bool show_all, bool show_values, bool minus_prefix = false) const
	{
		Ext show_caps = (show_all ? ~0 : capext.Get(user));

		for (const auto& [_, cap] : caps)
		{
			if (!(show_caps & cap->GetMask()))
				continue;

			if ((show_all) && (!cap->OnList(user)))
				continue;

			std::string token;
			if (minus_prefix)
				token.push_back('-');
			token.append(cap->GetName());

			if (show_values)
			{
				const std::string* capvalue = cap->GetValue(user);
				if ((capvalue) && (!capvalue->empty()) && (capvalue->find(' ') == std::string::npos))
				{
					token.push_back('=');
					token.append(*capvalue, 0, MAX_VALUE_LENGTH);
				}
			}
			out.push_back(token);
		}
	}

	void HandleClear(LocalUser* user, std::vector<std::string>& result)
	{
		HandleList(result, user, false, false, true);
		capext.Unset(user);
	}
};

namespace
{
	std::string SerializeCaps(const Extensible* container, bool human)
	{
		// XXX: Cast away the const because IS_LOCAL() doesn't handle it
		LocalUser* user = IS_LOCAL(const_cast<User*>(static_cast<const User*>(container)));
		if (!user)
			return {};

		// List requested caps
		std::vector<std::string> result;
		managerimpl->HandleList(result, user, false, false);

		// Serialize cap protocol version. If building a human-readable string append a
		// new token, otherwise append only a single character indicating the version.
		std::string version;
		if (human)
			version.append("capversion=3.");
		switch (managerimpl->GetProtocol(user))
		{
			case Cap::CAP_302:
				version.push_back('2');
				break;
			default:
				version.push_back('1');
				break;
		}
		result.push_back(version);

		return insp::join(result, ' ');
	}
}

Cap::ExtItem::ExtItem(Module* mod)
	: IntExtItem(mod, "caps", ExtensionType::USER)
{
}

std::string Cap::ExtItem::ToHuman(const Extensible* container, void* item) const noexcept
{
	return SerializeCaps(container, true);
}

std::string Cap::ExtItem::ToInternal(const Extensible* container, void* item) const noexcept
{
	return SerializeCaps(container, false);
}

void Cap::ExtItem::FromInternal(Extensible* container, const std::string& value) noexcept
{
	if (container->extype != this->extype)
		return;

	LocalUser* user = IS_LOCAL(static_cast<User*>(container));
	if (!user)
		return; // Can't happen

	// Process the cap protocol version which is a single character at the end of the serialized string
	if (value.back() == '2')
		managerimpl->Set302Protocol(user);

	// Remove the version indicator from the string passed to HandleReq
	std::string caplist(value, 0, value.size()-1);
	managerimpl->HandleReq(user, caplist);
}

class CapMessage final
	: public Cap::MessageBase
{
public:
	CapMessage(LocalUser* user, const std::string& subcmd, const std::string& result, bool asterisk)
		: Cap::MessageBase(subcmd)
	{
		SetUser(user);
		if (asterisk)
			PushParam("*");
		PushParamRef(result);
	}
};

class CommandCap final
	: public SplitCommand
{
private:
	Events::ModuleEventProvider evprov;
	Cap::ManagerImpl manager;
	ClientProtocol::EventProvider protoevprov;

	void DisplayResult(LocalUser* user, const std::string& subcmd, const std::vector<std::string>& result, bool asterisk)
	{
		size_t maxline = ServerInstance->Config->Limits.MaxLine - ServerInstance->Config->ServerName.size() - user->nick.length() - subcmd.length() - 11;
		std::string line;
		for (const auto& cap : result)
		{
			if (line.length() + cap.length() < maxline)
			{
				line.append(cap);
				line.push_back(' ');
			}
			else
			{
				DisplaySingleResult(user, subcmd, line, asterisk);
				line.clear();
			}
		}
		DisplaySingleResult(user, subcmd, line, false);
	}

	void DisplaySingleResult(LocalUser* user, const std::string& subcmd, const std::string& result, bool asterisk)
	{
		CapMessage msg(user, subcmd, result, asterisk);
		ClientProtocol::Event ev(protoevprov, msg);
		user->Send(ev);
	}

public:
	BoolExtItem holdext;

	CommandCap(Module* mod)
		: SplitCommand(mod, "CAP", 1)
		, evprov(mod, "event/cap")
		, manager(mod, evprov)
		, protoevprov(mod, name)
		, holdext(mod, "cap-hold", ExtensionType::USER)
	{
		works_before_reg = true;
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		if (!user->IsFullyConnected())
			holdext.Set(user);

		const std::string& subcommand = parameters[0];
		if (irc::equals(subcommand, "REQ"))
		{
			if (parameters.size() < 2)
				return CmdResult::FAILURE;

			const std::string replysubcmd = (manager.HandleReq(user, parameters[1]) ? "ACK" : "NAK");
			DisplaySingleResult(user, replysubcmd, parameters[1], false);
		}
		else if (irc::equals(subcommand, "END"))
		{
			holdext.Unset(user);
		}
		else if (irc::equals(subcommand, "LS") || irc::equals(subcommand, "LIST"))
		{
			Cap::Protocol capversion = Cap::CAP_LEGACY;
			const bool is_ls = (subcommand.length() == 2);
			if ((is_ls) && (parameters.size() > 1))
			{
				unsigned int version = ConvToNum<unsigned int>(parameters[1]);
				if (version >= 302)
				{
					capversion = Cap::CAP_302;
					manager.Set302Protocol(user);
				}
			}

			std::vector<std::string> result;
			// Show values only if supports v3.2 and doing LS
			manager.HandleList(result, user, is_ls, ((is_ls) && (capversion != Cap::CAP_LEGACY)));
			DisplayResult(user, subcommand, result, (capversion != Cap::CAP_LEGACY));
		}
		else if (irc::equals(subcommand, "CLEAR") && (manager.GetProtocol(user) == Cap::CAP_LEGACY))
		{
			std::vector<std::string> result;
			manager.HandleClear(user, result);
			DisplayResult(user, "ACK", result, false);
		}
		else
		{
			user->WriteNumeric(ERR_INVALIDCAPCMD, subcommand.empty() ? "*" : subcommand, "Invalid CAP subcommand");
			return CmdResult::FAILURE;
		}

		return CmdResult::SUCCESS;
	}
};

class PoisonCap final
	: public Cap::Capability
{
public:
	PoisonCap(Module* mod)
		: Cap::Capability(mod, "inspircd.org/poison")
	{
	}

	bool OnRequest(LocalUser* user, bool adding) override
	{
		// Reject the attempt to enable this capability.
		return false;
	}
};

class ModuleCap final
	: public Module
{
private:
	CommandCap cmd;
	PoisonCap poisoncap;

public:
	ModuleCap()
		: Module(VF_VENDOR, "Provides support for the IRCv3 Client Capability Negotiation extension.")
		, cmd(this)
		, poisoncap(this)
	{
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		return (cmd.holdext.Get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU);
	}
};

MODULE_INIT(ModuleCap)
