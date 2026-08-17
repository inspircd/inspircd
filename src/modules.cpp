/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 aasb13 <as_above_so_below31@proton.me>
 *   Copyright (C) 2019 iwalkalone <iwalkalone69@gmail.com>
 *   Copyright (C) 2017-2026 Sadie Powell <sadie@sadiepowell.dev>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@gmail.com>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Oliver Lupton <om@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2009 Craig Edwards <brain@inspircd.org>
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


#include <fmt/color.h>

#include "inspircd.h"
#include "dynamic.h"
#include "utility/container.h"

Module::Module(Module::Properties mprops, const std::string& mdesc)
	: Module(mprops, "", mdesc)
{
}

Module::Module(Module::Properties mprops, const std::string& mversion, const std::string& mdesc)
	: pointer(this, Cullable::Deleter())
	, description(mdesc)
	, properties(mprops)
	, version(mversion)
{
}

Cullable::Result Module::Cull()
{
	if (ModuleDLL)
	{
		ServerInstance->GlobalCulls.AddItem(ModuleDLL);
		ModuleDLL = nullptr;
	}
	return Cullable::Cull();
}

void Module::Cleanup(ExtensionType type, Extensible* item)
{
	// Intentionally left blank.
}

void Module::CompareLinkData(const LinkData& otherdata, LinkDataDiff& diffs)
{
	LinkData data;
	this->GetLinkData(data);
	insp::map_difference(data, otherdata, diffs);
}

void Module::GetLinkData(LinkData&)
{
	// Intentionally left blank.
}

std::string Module::GetPropertyString() const
{
	// V = VENDOR
	// R = CORE ("required")
	// C = COMMON
	// O = OPTCOMMON
	// D = DEPRECATED
	std::string propstr("VRCOD");
	size_t pos = 0;
	for (int mult = Module::FIRST; mult <= Module::LAST; mult *= 2, ++pos)
		if (!(this->properties & mult))
			propstr[pos] = '-';
	return propstr;
}

std::string Module::GetVersion() const
{
	if (!version.empty())
		return version;

	const auto* dll_version = ModuleDLL->GetVersion();
	return dll_version ? dll_version : "unknown";
}

void Module::Prioritize()
{
	// Intentionally left blank.
}

void Module::ReadConfig(ConfigStatus& status)
{
	// Intentionally left blank.
}

ModulePtr Module::Share()
{
	if (!this->pointer) [[unlikely]]
		throw CoreException("Module::Share called on an already shared module.");

	ModulePtr newpointer;
	std::swap(this->pointer, newpointer);
	return newpointer;
}

#define DEFAULT_MODULE_EVENT_RETURN_ModResult MOD_RES_PASSTHRU
#define DEFAULT_MODULE_EVENT_RETURN_void
#define DEFAULT_MODULE_EVENT(NAME, RETURN, ...) \
	RETURN Module:: NAME ( __VA_ARGS__ ) \
	{ \
		ServerInstance->Modules.Detach(I_ ## NAME, shared_from_this()); \
		return DEFAULT_MODULE_EVENT_RETURN_ ## RETURN; \
	}

//                   NAME                       RETURN      ARGS...
DEFAULT_MODULE_EVENT(OnAcceptConnection,        ModResult,  int, ListenSocket*, const irc::sockets::sockaddrs&, const irc::sockets::sockaddrs&);
DEFAULT_MODULE_EVENT(OnAddLine,                 void,       User*, XLine*);
DEFAULT_MODULE_EVENT(OnBackgroundTimer,         void,       time_t);
DEFAULT_MODULE_EVENT(OnBuildNeighborList,       void,       User*, User::NeighborList&, User::NeighborExceptions&);
DEFAULT_MODULE_EVENT(OnChangeConnectClass,      void,       LocalUser*, const std::shared_ptr<ConnectClass>&, bool);
DEFAULT_MODULE_EVENT(OnChangeHost,              void,       User*, const std::string&);
DEFAULT_MODULE_EVENT(OnChangeRealHost,          void,       User*, const std::string&);
DEFAULT_MODULE_EVENT(OnChangeRealName,          void,       User*, const std::string&);
DEFAULT_MODULE_EVENT(OnChangeRealUser,          void,       User*, const std::string&);
DEFAULT_MODULE_EVENT(OnChangeRemoteAddress,     void,       LocalUser*);
DEFAULT_MODULE_EVENT(OnChangeUser,              void,       User*, const std::string&);
DEFAULT_MODULE_EVENT(OnChannelDelete,           void,       Channel*);
DEFAULT_MODULE_EVENT(OnChannelPreDelete,        ModResult,  Channel*);
DEFAULT_MODULE_EVENT(OnCheckInvite,             ModResult,  User*, Channel*);
DEFAULT_MODULE_EVENT(OnCheckKey,                ModResult,  User*, Channel*, const std::string&);
DEFAULT_MODULE_EVENT(OnCheckLimit,              ModResult,  User*, Channel*);
DEFAULT_MODULE_EVENT(OnCheckList,               ModResult,  ListModeBase* lm, User*, Channel*, bool);
DEFAULT_MODULE_EVENT(OnCheckListEntry,          ModResult,  ListModeBase* lm, User*, Channel*, const std::string&, bool);
DEFAULT_MODULE_EVENT(OnCheckReady,              ModResult,  LocalUser*);
DEFAULT_MODULE_EVENT(OnCommandBlocked,          void,       const std::string&, const CommandBase::Params&, LocalUser*);
DEFAULT_MODULE_EVENT(OnDecodeMetadata,          void,       Extensible*, const std::string&, const std::string&);
DEFAULT_MODULE_EVENT(OnDelLine,                 void,       User*, XLine*);
DEFAULT_MODULE_EVENT(OnExpireLine,              void,       XLine*);
DEFAULT_MODULE_EVENT(OnGarbageCollect,          void);
DEFAULT_MODULE_EVENT(OnKill,                    ModResult,  User*, User*, const std::string&);
DEFAULT_MODULE_EVENT(OnLoadModule,              void,       const ModulePtr&);
DEFAULT_MODULE_EVENT(OnMode,                    void,       User*, User*, Channel*, const Modes::ChangeList&, ModeParser::ModeProcessFlag);
DEFAULT_MODULE_EVENT(OnModuleRehash,            void,       User*, const std::string&);
DEFAULT_MODULE_EVENT(OnNumeric,                 ModResult,  User*, Numeric::Numeric&);
DEFAULT_MODULE_EVENT(OnOperLogin,               void,       User*, const std::shared_ptr<OperAccount>&, bool);
DEFAULT_MODULE_EVENT(OnOperLogout,              void,       User*);
DEFAULT_MODULE_EVENT(OnPostChangeConnectClass,  void,       LocalUser*, const std::shared_ptr<ConnectClass>&, bool);
DEFAULT_MODULE_EVENT(OnPostChangeRealHost,      void,       User*);
DEFAULT_MODULE_EVENT(OnPostChangeRealUser,      void,       User*);
DEFAULT_MODULE_EVENT(OnPostCommand,             void,       Command*, const CommandBase::Params&, LocalUser*, CmdResult, bool);
DEFAULT_MODULE_EVENT(OnPostConnect,             void,       User*);
DEFAULT_MODULE_EVENT(OnPostJoin,                void,       Membership*);
DEFAULT_MODULE_EVENT(OnPostOperLogin,           void,       User*, bool);
DEFAULT_MODULE_EVENT(OnPostOperLogout,          void,       User*, const std::shared_ptr<OperAccount>&);
DEFAULT_MODULE_EVENT(OnPostTopicChange,         void,       User*, Channel*, const std::string&);
DEFAULT_MODULE_EVENT(OnPreChangeConnectClass,   ModResult,  LocalUser*, const std::shared_ptr<ConnectClass>&, std::optional<Numeric::Numeric>&);
DEFAULT_MODULE_EVENT(OnPreCommand,              ModResult,  std::string&, CommandBase::Params&, LocalUser*, bool);
DEFAULT_MODULE_EVENT(OnPreMode,                 ModResult,  User*, User*, Channel*, Modes::ChangeList&);
DEFAULT_MODULE_EVENT(OnPreOperLogin,            ModResult,  LocalUser*, const std::shared_ptr<OperAccount>&, bool);
DEFAULT_MODULE_EVENT(OnPreRehash,               void,       User*, const std::string&);
DEFAULT_MODULE_EVENT(OnPreTopicChange,          ModResult,  User*, Channel*, const std::string&);
DEFAULT_MODULE_EVENT(OnRawMode,                 ModResult,  User*, Channel*, const Modes::Change&);
DEFAULT_MODULE_EVENT(OnSendSnotice,             ModResult,  char& snomask, std::string& type, const std::string& message);
DEFAULT_MODULE_EVENT(OnServiceAdd,              void,       Service::Provider&);
DEFAULT_MODULE_EVENT(OnServiceDel,              void,       Service::Provider&);
DEFAULT_MODULE_EVENT(OnShutdown,                void,       const std::string& reason, bool);
DEFAULT_MODULE_EVENT(OnUnloadModule,            void,       const ModulePtr&);
DEFAULT_MODULE_EVENT(OnUserConnect,             void,       LocalUser*);
DEFAULT_MODULE_EVENT(OnUserDisconnect,          void,       LocalUser*);
DEFAULT_MODULE_EVENT(OnUserInit,                void,       LocalUser*);
DEFAULT_MODULE_EVENT(OnUserInvite,              void,       User*, User*, Channel*, time_t, ModeHandler::Rank, User::List&);
DEFAULT_MODULE_EVENT(OnUserJoin,                void,       Membership*, bool, bool, User::List&);
DEFAULT_MODULE_EVENT(OnUserKick,                void,       User*, Membership*, const std::string&, User::List&);
DEFAULT_MODULE_EVENT(OnUserMessage,             void,       User*, const MessageTarget&, const MessageDetails&);
DEFAULT_MODULE_EVENT(OnUserMessageBlocked,      void,       User*, const MessageTarget&, const MessageDetails&);
DEFAULT_MODULE_EVENT(OnUserPart,                void,       Membership*, std::string&, User::List&);
DEFAULT_MODULE_EVENT(OnUserPostInit,            void,       LocalUser*);
DEFAULT_MODULE_EVENT(OnUserPostMessage,         void,       User*, const MessageTarget&, const MessageDetails&);
DEFAULT_MODULE_EVENT(OnUserPostNick,            void,       User*, const std::string&);
DEFAULT_MODULE_EVENT(OnUserPreInvite,           ModResult,  User*, User*, Channel*, time_t);
DEFAULT_MODULE_EVENT(OnUserPreJoin,             ModResult,  LocalUser*, Channel*, const std::string&, PrefixMode::Set&, const std::string&, bool);
DEFAULT_MODULE_EVENT(OnUserPreKick,             ModResult,  User*, Membership*, const std::string&);
DEFAULT_MODULE_EVENT(OnUserPreMessage,          ModResult,  User*, MessageTarget&, MessageDetails&);
DEFAULT_MODULE_EVENT(OnUserPreNick,             ModResult,  LocalUser*, const std::string&);
DEFAULT_MODULE_EVENT(OnUserPreQuit,             ModResult,  LocalUser*, std::string&, std::string&);
DEFAULT_MODULE_EVENT(OnUserQuit,                void,       User*, const std::string&, const std::string&);
DEFAULT_MODULE_EVENT(OnUserRegister,            ModResult,  LocalUser*);
DEFAULT_MODULE_EVENT(OnUserWrite,               ModResult,  LocalUser*, ClientProtocol::Message&);

#undef DEFAULT_MODULE_EVENT
#undef DEFAULT_MODULE_EVENT_RETURN_void
#undef DEFAULT_MODULE_EVENT_RETURN_ModResult

bool ModuleManager::Attach(Implementation i, const ModulePtr& mod)
{
	if (insp::contains(EventHandlers[i], mod))
		return false;

	EventHandlers[i].push_back(mod);
	return true;
}

bool ModuleManager::Detach(Implementation i, const ModulePtr& mod)
{
	return std::erase(EventHandlers[i], mod);
}

void ModuleManager::Attach(const Implementation* i, const ModulePtr& mod, size_t sz)
{
	for (size_t n = 0; n < sz; ++n)
		Attach(i[n], mod);
}

void ModuleManager::Detach(const Implementation* i, const ModulePtr& mod, size_t sz)
{
	for (size_t n = 0; n < sz; ++n)
		Detach(i[n], mod);
}

void ModuleManager::AttachAll(const ModulePtr& mod)
{
	for (size_t i = 0; i != I_END; ++i)
		Attach(static_cast<Implementation>(i), mod);
}

void ModuleManager::DetachAll(const ModulePtr& mod)
{
	for (size_t n = 0; n != I_END; ++n)
		Detach(static_cast<Implementation>(n), mod);
}

void ModuleManager::SetPriority(const ModulePtr& mod, Module::Priority s)
{
	for (size_t n = 0; n != I_END; ++n)
		SetPriority(mod, static_cast<Implementation>(n), s);
}

bool ModuleManager::SetPriority(const ModulePtr& mod, Implementation i, Module::Priority s, const std::string& which)
{
	ModulePtr depmod;
	if (!which.empty())
		depmod = ServerInstance->Modules.Find(which);
	return SetPriority(mod, i, s, depmod);
}

bool ModuleManager::SetPriority(const ModulePtr& mod, Implementation i, Module::Priority s, const ModulePtr& which)
{
	/** To change the priority of a module, we first find its position in the vector,
	 * then we find the position of the other modules in the vector that this module
	 * wants to be before/after. We pick off either the first or last of these depending
	 * on which they want, and we make sure our module is *at least* before or after
	 * the first or last of this subset, depending again on the type of priority.
	 */
	size_t my_pos = 0;

	/* Locate our module. This is O(n) but it only occurs on module load so we're
	 * not too bothered about it
	 */
	for (size_t x = 0; x != EventHandlers[i].size(); ++x)
	{
		if (EventHandlers[i][x] == mod)
		{
			my_pos = x;
			goto found_src;
		}
	}

	/* Eh? this module doesnt exist, probably trying to set priority on an event
	 * they're not attached to.
	 */
	return false;

found_src:
	// The modules registered for a hook are called in reverse order (to allow for easier removal
	// of list entries while looping), meaning that the Priority given to us has the exact opposite effect
	// on the list, e.g.: PRIORITY_BEFORE will actually put 'mod' after 'which', etc.
	size_t swap_pos;
	switch (s)
	{
		case Module::PRIORITY_LAST:
		{
			if (prioritizationState != PRIO_STATE_FIRST)
				return true;

			swap_pos = 0;
			break;
		}

		case Module::PRIORITY_FIRST:
		{
			if (prioritizationState != PRIO_STATE_FIRST)
				return true;

			swap_pos = EventHandlers[i].size() - 1;
			break;
		}

		case Module::PRIORITY_BEFORE:
		{
			/* Find the latest possible position, only searching AFTER our position */
			for (size_t x = EventHandlers[i].size() - 1; x > my_pos; --x)
			{
				if (EventHandlers[i][x] == which)
				{
					swap_pos = x;
					goto swap_now;
				}
			}
			// didn't find it - either not loaded or we're already after
			return true;
		}

		/* Place this module before a set of other modules */
		case Module::PRIORITY_AFTER:
		{
			for (size_t x = 0; x < my_pos; ++x)
			{
				if (EventHandlers[i][x] == which)
				{
					swap_pos = x;
					goto swap_now;
				}
			}
			// didn't find it - either not loaded or we're already before
			return true;
		}

		default:
			return true; // Should never happen.
	}

swap_now:
	/* Do we need to swap? */
	if (swap_pos != my_pos)
	{
		// We are going to change positions; we'll need to run again to verify all requirements
		if (prioritizationState == PRIO_STATE_LAST)
			prioritizationState = PRIO_STATE_AGAIN;
		/* Suggestion from Phoenix, "shuffle" the modules to better retain call order */
		int increment = 1;

		if (my_pos > swap_pos)
			increment = -1;

		for (size_t j = my_pos; j != swap_pos; j += increment)
		{
			if ((j + increment > EventHandlers[i].size() - 1) || ((increment == -1) && (j == 0)))
				continue;

			std::swap(EventHandlers[i][j], EventHandlers[i][j+increment]);
		}
	}

	return true;
}

bool ModuleManager::PrioritizeHooks()
{
	/* We give every module a chance to re-prioritize when we introduce a new one,
	 * not just the one that's loading, as the new module could affect the preference
	 * of others
	 */
	for (int tries = 0; tries < 20; tries++)
	{
		prioritizationState = tries > 0 ? PRIO_STATE_LAST : PRIO_STATE_FIRST;
		for (const auto& [_, mod] : Modules)
			mod->Prioritize();

		if (prioritizationState == PRIO_STATE_LAST)
			break;
		if (tries == 19)
		{
			ServerInstance->Logs.Debug("MODULE", "Hook priority dependency loop detected");
			return false;
		}
	}
	return true;
}

bool ModuleManager::CanUnload(const ModulePtr& mod)
{
	auto modfind = Modules.find(mod->ModuleFile);
	if ((modfind == Modules.end()) || (modfind->second != mod) || (mod->dying))
	{
		LastModuleError = "Module " + mod->ModuleFile + " is not loaded, cannot unload it!";
		ServerInstance->Logs.Critical("MODULE", LastModuleError);
		return false;
	}

	mod->dying = true;
	return true;
}

void ModuleManager::UnregisterModes(const ModulePtr& mod, ModeType modetype)
{
	const ModeParser::ModeHandlerMap& modes = ServerInstance->Modes.GetModes(modetype);
	for (ModeParser::ModeHandlerMap::const_iterator i = modes.begin(); i != modes.end(); )
	{
		ModeHandler* const mh = i->second;
		++i;
		if (insp::same_ptr(mh->service_creator, mod))
			this->DelService(*mh);
	}
}

void ModuleManager::DoSafeUnload(const ModulePtr& mod)
{
	// First, notify all modules that a module is about to be unloaded, so in case
	// they pass execution to the soon to be unloaded module, it will happen now,
	// i.e. before we unregister the services of the module being unloaded
	FOREACH_MOD(OnUnloadModule, (mod));

	auto modfind = Modules.find(mod->ModuleFile);

	// Unregister modes before extensions because modes may require their extension to show the mode being unset
	UnregisterModes(mod, MODETYPE_USER);
	UnregisterModes(mod, MODETYPE_CHANNEL);

	std::vector<ExtensionItem*> items;
	ServerInstance->Extensions.BeginUnregister(modfind->second, items);
	/* Give the module a chance to tidy out all its metadata */
	const ChannelMap& chans = ServerInstance->Channels.GetChans();
	for (ChannelMap::const_iterator c = chans.begin(); c != chans.end(); )
	{
		Channel* chan = c->second;
		++c;
		mod->Cleanup(ExtensionType::CHANNEL, chan);
		chan->UnhookExtensions(items);
		for (const auto& [_, memb] : chan->GetUsers())
		{
			mod->Cleanup(ExtensionType::MEMBERSHIP, memb);
			memb->UnhookExtensions(items);
		}
	}

	const UserMap& users = ServerInstance->Users.GetUsers();
	for (UserMap::const_iterator u = users.begin(); u != users.end(); )
	{
		User* user = u->second;
		// The module may quit the user (e.g. TLS mod unloading) and that will remove it from the container
		++u;
		mod->Cleanup(ExtensionType::USER, user);
		user->UnhookExtensions(items);
	}

	for (auto i = this->Services.begin(); i != this->Services.end(); )
	{
		auto curr = i++;
		if (insp::same_ptr(curr->second->service_creator, mod))
		{
			auto* service = curr->second;
			this->Services.erase(curr);
			FOREACH_MOD(OnServiceDel, (*service));
		}
	}

	dynamic_reference_base::reset_all();

	DetachAll(mod);

	Modules.erase(modfind);

	ServerInstance->Logs.Normal("MODULE", "The {} module was unloaded", mod->ModuleFile);
}

void ModuleManager::UnloadAll()
{
	/* We do this more than once, so that any service providers get a
	 * chance to be unhooked by the modules using them, but then get
	 * a chance to be removed themselves.
	 *
	 * Note: this deliberately does NOT delete the DLLManager objects
	 */
	for (int tries = 0; tries < 4; tries++)
	{
		auto i = Modules.begin();
		while (i != Modules.end())
		{
			auto me = i++;
			if (CanUnload(me->second))
			{
				DoSafeUnload(me->second);
			}
		}
		ServerInstance->GlobalCulls.Apply();
	}
}

namespace
{
	struct UnloadAction final
		: public ActionBase
	{
		const ModulePtr mod;
		UnloadAction(const ModulePtr& m)
			: mod(m)
		{
		}
		void Call() override
		{
			ServerInstance->Modules.DoSafeUnload(mod);
			ServerInstance->GlobalCulls.Apply();
			ServerInstance->GlobalCulls.AddItem(this);
		}
	};
}

bool ModuleManager::Unload(const ModulePtr& mod)
{
	if (!CanUnload(mod))
		return false;
	ServerInstance->AtomicActions.AddAction(new UnloadAction(mod));
	return true;
}

void ModuleManager::LoadAll()
{
	std::map<std::string, Service::List> servicemap;
	LoadCoreModules(servicemap);

	// Step 1: load all of the modules.
	for (const auto& shortname : ServerInstance->Config->GetModules())
	{
		// Skip modules which are already loaded.
		const std::string name = ExpandModName(shortname);
		if (Modules.find(name) != Modules.end())
			continue;

		this->NewServices = &servicemap[name];
		fmt::println("[{}] Loading module:\t{}", fmt::styled("*", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::green)), name);
		if (!this->Load(name, true))
		{
			fmt::println("[{}] {}", fmt::styled("*", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)), LastError());
			ServerInstance->Exit(EXIT_FAILURE, "A module failed to load");
		}
	}

	// Step 2: initialize the modules and register their services.
	for (const auto& [modname, mod] : Modules)
	{
		try
		{
			ServerInstance->Logs.Debug("MODULE", "Initializing {}", modname);
			AttachAll(mod);
			AddServices(servicemap[modname]);
			mod->init();
		}
		catch (const CoreException& modexcept)
		{
			LastModuleError = "Unable to initialize " + modname + ": " + modexcept.GetReason();
			ServerInstance->Logs.Critical("MODULE", LastModuleError);
			fmt::println("[{}] {}", fmt::styled("*", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)), LastModuleError);
			ServerInstance->Exit(EXIT_FAILURE, "A module failed to initialize");
		}
	}

	this->NewServices = nullptr;
	ConfigStatus confstatus(nullptr, true);

	// Step 3: Read the configuration for the modules. This must be done as part of
	// its own step so that services provided by modules can be registered before
	// the configuration is read.
	for (const auto& [modname, mod] : Modules)
	{
		try
		{
			ServerInstance->Logs.Debug("MODULE", "Reading configuration for {}", modname);
			mod->ReadConfig(confstatus);
		}
		catch (const CoreException& modexcept)
		{
			LastModuleError = "Unable to read the configuration for " + modname + ": " + modexcept.GetReason();
			ServerInstance->Logs.Critical("MODULE", LastModuleError);
			fmt::println("[{}] {}", fmt::styled("*", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)), LastModuleError);
			ServerInstance->Exit(EXIT_FAILURE, "A module has invalid configuration");
		}
	}

	if (!PrioritizeHooks())
		ServerInstance->Exit(EXIT_FAILURE, "Unable to prioritise module hooks", "MODULE");
}

std::string& ModuleManager::LastError()
{
	return LastModuleError;
}

void ModuleManager::AddServices(const Service::List& list)
{
	for (auto* service : list)
		AddService(*service);
}

void ModuleManager::AddService(Service::Provider& item)
{
#ifndef NDEBUG
	ServerInstance->Logs.Debug("SERVICE", "Adding {} {} ({}) provided by {}", item.service_type,
		item.service_name, (void*)&item, item.GetSource());
#endif

	item.RegisterService();
	FOREACH_MOD(OnServiceAdd, (item));
}

void ModuleManager::DelService(Service::Provider& item)
{
#ifndef NDEBUG
	ServerInstance->Logs.Debug("SERVICE", "Deleting {} {} ({}) provided by {}", item.service_type,
		item.service_name, (void*)&item, item.GetSource());
#endif

	item.UnregisterService();
	FOREACH_MOD(OnServiceDel, (item));
}

Service::Provider* ModuleManager::FindService(const std::string& type, const std::string& name)
{
	return insp::find_value(this->Services, std::make_pair(type, name));
}

std::string ModuleManager::ExpandModName(const std::string& modname)
{
	const static size_t extlen = strlen(INSPIRCD_MODULE_EXT);
	std::string fullname;
	if (modname.compare(0, 5, "core_") != 0 && modname.compare(0, 2, "m_") != 0)
		fullname.append("m_");
	fullname.append(modname);
	if (modname.length() < extlen || modname.compare(modname.size() - extlen, extlen, INSPIRCD_MODULE_EXT) != 0)
		fullname.append(INSPIRCD_MODULE_EXT);
	return fullname;
}

std::string ModuleManager::ShrinkModName(const std::string& modname)
{
	const static size_t extlen = strlen(INSPIRCD_MODULE_EXT);
	size_t startpos = modname.compare(0, 2, "m_", 2) ? 0 : 2;
	size_t endpos = modname.length() < extlen || modname.compare(modname.length() - extlen, extlen, INSPIRCD_MODULE_EXT, extlen) ? 0 : extlen;
	return modname.substr(startpos, modname.length() - endpos - startpos);
}

ModulePtr ModuleManager::Find(const std::string& name)
{
	auto modfind = Modules.find(ExpandModName(name));
	if (modfind == Modules.end())
		return nullptr;
	else
		return modfind->second;
}

void ModuleManager::AddReferent(const std::string& stype, const std::string& sname, Service::Provider* service)
{
#ifndef NDEBUG
	ServerInstance->Logs.Debug("SERVICE", "Adding reference to {} as {} {}",
		(void*)service, stype, sname);
#endif
	this->Services.emplace(std::make_pair(stype, sname), service);
	dynamic_reference_base::reset_all(stype);
}

void ModuleManager::DelReferent(Service::Provider* service)
{
	for (auto i = this->Services.begin(); i != this->Services.end(); )
	{
		Service::Provider* curr = i->second;
		if (curr == service)
		{
#ifndef NDEBUG
			ServerInstance->Logs.Debug("SERVICE", "Deleting reference to {} as {} {}",
				(void*)service, curr->service_type, curr->service_name);
#endif
			this->Services.erase(i++);
		}
		else
			++i;
	}
	dynamic_reference_base::reset_all();
}
