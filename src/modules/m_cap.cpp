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
#include "modules/cap.h"

namespace Cap
{
	class ManagerImpl;
}

class Cap::ManagerImpl : public Cap::Manager
{
	typedef insp::flat_map<std::string, Capability*, irc::insensitive_swo> CapMap;

	ExtItem capext;
	CapMap caps;

	static bool CanRequest(LocalUser* user, Ext usercaps, Capability* cap, bool adding)
	{
		if ((usercaps & cap->GetMask()) == adding)
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

 public:
	ManagerImpl(Module* mod)
		: Cap::Manager(mod)
		, capext("caps", ExtensionItem::EXT_USER, mod)
	{
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
	}

	void DelCap(Cap::Capability* cap) CXX11_OVERRIDE
	{
		// No-op if the cap is not registered, see AddCap() above
		if (!cap->IsRegistered())
			return;

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Unregistering cap %s", cap->GetName().c_str());

		// Turn off the cap for all users
		const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
		for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); ++i)
		{
			LocalUser* user = *i;
			cap->set(user, false);
		}

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

	void HandleList(std::string& out, LocalUser* user, bool show_all, bool minus_prefix = false) const
	{
		Ext show_caps = (show_all ? ~0 : capext.get(user));

		for (CapMap::const_iterator i = caps.begin(); i != caps.end(); ++i)
		{
			Capability* cap = i->second;
			if (!(show_caps & cap->GetMask()))
				continue;

			if (minus_prefix)
				out.push_back('-');
			out.append(cap->GetName()).push_back(' ');
		}
	}

	void HandleClear(LocalUser* user, std::string& result)
	{
		HandleList(result, user, false, true);
		capext.unset(user);
	}
};

class CommandCap : public SplitCommand
{
	Cap::ManagerImpl manager;

	static void DisplayResult(LocalUser* user, std::string& result)
	{
		if (result.size() > 5)
			result.erase(result.end()-1);
		user->WriteCommand("CAP", result);
	}

 public:
	LocalIntExt holdext;

	CommandCap(Module* mod)
		: SplitCommand(mod, "CAP", 1)
		, manager(mod)
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

			std::string result = subcommand + " :";
			manager.HandleList(result, user, is_ls);
			DisplayResult(user, result);
		}
		else if (subcommand == "CLEAR")
		{
			std::string result = "ACK :";
			manager.HandleClear(user, result);
			DisplayResult(user, result);
		}
		else
		{
			user->WriteNumeric(ERR_INVALIDCAPSUBCOMMAND, "%s :Invalid CAP subcommand", subcommand.c_str());
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
