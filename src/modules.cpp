/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 iwalkalone <iwalkalone69@gmail.com>
 *   Copyright (C) 2017-2024 Sadie Powell <sadie@witchery.services>
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
#include "utility/map.h"

static insp::intrusive_list<dynamic_reference_base>* dynrefs = nullptr;

void dynamic_reference_base::reset_all()
{
	if (!dynrefs)
		return;

	for (auto* dynref : *dynrefs)
		dynref->resolve();
}

Module::Module(int mprops, const std::string& mdesc)
	: Module(mprops, "", mdesc)
{
}

Module::Module(int mprops, const std::string& mversion, const std::string& mdesc)
	: description(mdesc)
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

void Module::CompareLinkData(const LinkData& otherdata, LinkDataDiff& diffs)
{
	std::string unused;
	LinkData data;
	this->GetLinkData(data, unused);
	insp::map::difference(data, otherdata, diffs);
}

std::string Module::GetPropertyString() const
{
	// D = VF_CORE ("default")
	// V = VF_VENDOR
	// C = VF_COMMON
	// O = VF_OPTCOMMON
	std::string propstr("DVCO");
	size_t pos = 0;
	for (int mult = VF_CORE; mult <= VF_OPTCOMMON; mult *= 2, ++pos)
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

void Module::DetachEvent(Implementation i)
{
	ServerInstance->Modules.Detach(i, this);
}

void		Module::GetLinkData(LinkData&, std::string&) { }
void		Module::Prioritize() { }
void		Module::ReadConfig(ConfigStatus& status) { }
ModResult	Module::OnSendSnotice(char& snomask, std::string& type, const std::string& message) { DetachEvent(I_OnSendSnotice); return MOD_RES_PASSTHRU; }
void		Module::OnUserConnect(LocalUser*) { DetachEvent(I_OnUserConnect); }
ModResult	Module::OnUserPreQuit(LocalUser*, std::string&, std::string&) { DetachEvent(I_OnUserPreQuit); return MOD_RES_PASSTHRU; }
void		Module::OnUserQuit(User*, const std::string&, const std::string&) { DetachEvent(I_OnUserQuit); }
void		Module::OnUserDisconnect(LocalUser*) { DetachEvent(I_OnUserDisconnect); }
void		Module::OnUserJoin(Membership*, bool, bool, CUList&) { DetachEvent(I_OnUserJoin); }
void		Module::OnPostJoin(Membership*) { DetachEvent(I_OnPostJoin); }
void		Module::OnUserPart(Membership*, std::string&, CUList&) { DetachEvent(I_OnUserPart); }
void		Module::OnPreRehash(User*, const std::string&) { DetachEvent(I_OnPreRehash); }
void		Module::OnModuleRehash(User*, const std::string&) { DetachEvent(I_OnModuleRehash); }
ModResult	Module::OnUserPreJoin(LocalUser*, Channel*, const std::string&, std::string&, const std::string&, bool) { DetachEvent(I_OnUserPreJoin); return MOD_RES_PASSTHRU; }
void		Module::OnMode(User*, User*, Channel*, const Modes::ChangeList&, ModeParser::ModeProcessFlag) { DetachEvent(I_OnMode); }
ModResult	Module::OnUserPreInvite(User*, User*, Channel*, time_t) { DetachEvent(I_OnUserPreInvite); return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreMessage(User*, MessageTarget&, MessageDetails&) { DetachEvent(I_OnUserPreMessage); return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreNick(LocalUser*, const std::string&) { DetachEvent(I_OnUserPreNick); return MOD_RES_PASSTHRU; }
void		Module::OnUserPostNick(User*, const std::string&) { DetachEvent(I_OnUserPostNick); }
ModResult	Module::OnPreMode(User*, User*, Channel*, Modes::ChangeList&) { DetachEvent(I_OnPreMode); return MOD_RES_PASSTHRU; }
ModResult	Module::OnKill(User*, User*, const std::string&) { DetachEvent(I_OnKill); return MOD_RES_PASSTHRU; }
void		Module::OnLoadModule(Module*) { DetachEvent(I_OnLoadModule); }
void		Module::OnUnloadModule(Module*) { DetachEvent(I_OnUnloadModule); }
void		Module::OnBackgroundTimer(time_t) { DetachEvent(I_OnBackgroundTimer); }
ModResult	Module::OnPreCommand(std::string&, CommandBase::Params&, LocalUser*, bool) { DetachEvent(I_OnPreCommand); return MOD_RES_PASSTHRU; }
void		Module::OnPostCommand(Command*, const CommandBase::Params&, LocalUser*, CmdResult, bool) { DetachEvent(I_OnPostCommand); }
void		Module::OnCommandBlocked(const std::string&, const CommandBase::Params&, LocalUser*) { DetachEvent(I_OnCommandBlocked); }
void		Module::OnUserInit(LocalUser*) { DetachEvent(I_OnUserInit); }
void		Module::OnUserPostInit(LocalUser*) { DetachEvent(I_OnUserPostInit); }
ModResult	Module::OnCheckReady(LocalUser*) { DetachEvent(I_OnCheckReady); return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserRegister(LocalUser*) { DetachEvent(I_OnUserRegister); return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreKick(User*, Membership*, const std::string&) { DetachEvent(I_OnUserPreKick); return MOD_RES_PASSTHRU; }
void		Module::OnUserKick(User*, Membership*, const std::string&, CUList&) { DetachEvent(I_OnUserKick); }
ModResult	Module::OnRawMode(User*, Channel*, const Modes::Change&) { DetachEvent(I_OnRawMode); return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckInvite(User*, Channel*) { DetachEvent(I_OnCheckInvite); return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckKey(User*, Channel*, const std::string&) { DetachEvent(I_OnCheckKey); return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckLimit(User*, Channel*) { DetachEvent(I_OnCheckLimit); return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckChannelBan(User*, Channel*) { DetachEvent(I_OnCheckChannelBan); return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckBan(User*, Channel*, const std::string&) { DetachEvent(I_OnCheckBan); return MOD_RES_PASSTHRU; }
ModResult	Module::OnPreTopicChange(User*, Channel*, const std::string&) { DetachEvent(I_OnPreTopicChange); return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckPassword(const std::string&, const std::string&, const std::string&) { DetachEvent(I_OnCheckPassword); return MOD_RES_PASSTHRU; }
void		Module::OnPostConnect(User*) { DetachEvent(I_OnPostConnect); }
void		Module::OnUserPostMessage(User*, const MessageTarget&, const MessageDetails&) { DetachEvent(I_OnUserPostMessage); }
void		Module::OnUserMessageBlocked(User*, const MessageTarget&, const MessageDetails&) { DetachEvent(I_OnUserMessageBlocked); }
void		Module::OnUserInvite(User*, User*, Channel*, time_t, ModeHandler::Rank, CUList&) { DetachEvent(I_OnUserInvite); }
void		Module::OnPostTopicChange(User*, Channel*, const std::string&) { DetachEvent(I_OnPostTopicChange); }
void		Module::OnDecodeMetadata(Extensible*, const std::string&, const std::string&) { DetachEvent(I_OnDecodeMetadata); }
void		Module::OnChangeHost(User*, const std::string&) { DetachEvent(I_OnChangeHost); }
void		Module::OnChangeRealHost(User*, const std::string&) { DetachEvent(I_OnChangeRealHost); }
void		Module::OnPostChangeRealHost(User*) { DetachEvent(I_OnPostChangeRealHost); }
void		Module::OnChangeRealName(User*, const std::string&) { DetachEvent(I_OnChangeRealName); }
void		Module::OnChangeUser(User*, const std::string&) { DetachEvent(I_OnChangeUser); }
void		Module::OnChangeRealUser(User*, const std::string&) { DetachEvent(I_OnChangeRealUser); }
void		Module::OnPostChangeRealUser(User*) { DetachEvent(I_OnPostChangeRealUser); }
void		Module::OnAddLine(User*, XLine*) { DetachEvent(I_OnAddLine); }
void		Module::OnDelLine(User*, XLine*) { DetachEvent(I_OnDelLine); }
void		Module::OnExpireLine(XLine*) { DetachEvent(I_OnExpireLine); }
void		Module::OnCleanup(ExtensionType, Extensible*) { }
ModResult	Module::OnChannelPreDelete(Channel*) { DetachEvent(I_OnChannelPreDelete); return MOD_RES_PASSTHRU; }
void		Module::OnChannelDelete(Channel*) { DetachEvent(I_OnChannelDelete); }
void		Module::OnBuildNeighborList(User*, User::NeighborList&, User::NeighborExceptions&) { DetachEvent(I_OnBuildNeighborList); }
void		Module::OnGarbageCollect() { DetachEvent(I_OnGarbageCollect); }
void		Module::OnUserMessage(User*, const MessageTarget&, const MessageDetails&) { DetachEvent(I_OnUserMessage); }
ModResult	Module::OnNumeric(User*, const Numeric::Numeric&) { DetachEvent(I_OnNumeric); return MOD_RES_PASSTHRU; }
ModResult	Module::OnAcceptConnection(int, ListenSocket*, const irc::sockets::sockaddrs&, const irc::sockets::sockaddrs&) { DetachEvent(I_OnAcceptConnection); return MOD_RES_PASSTHRU; }
void		Module::OnChangeRemoteAddress(LocalUser*) { DetachEvent(I_OnChangeRemoteAddress); }
void		Module::OnServiceAdd(ServiceProvider&) { DetachEvent(I_OnServiceAdd); }
void		Module::OnServiceDel(ServiceProvider&) { DetachEvent(I_OnServiceDel); }
ModResult	Module::OnUserWrite(LocalUser*, ClientProtocol::Message&) { DetachEvent(I_OnUserWrite); return MOD_RES_PASSTHRU; }
void		Module::OnShutdown(const std::string& reason) { DetachEvent(I_OnShutdown); }
ModResult	Module::OnPreOperLogin(LocalUser*, const std::shared_ptr<OperAccount>&, bool) { DetachEvent(I_OnPreOperLogin); return MOD_RES_PASSTHRU; }
void		Module::OnOperLogin(User*, const std::shared_ptr<OperAccount>&, bool) { DetachEvent(I_OnOperLogin); }
void		Module::OnPostOperLogin(User*, bool) { DetachEvent(I_OnPostOperLogin); }
void		Module::OnOperLogout(User*) { DetachEvent(I_OnOperLogout); }
void		Module::OnPostOperLogout(User*, const std::shared_ptr<OperAccount>&) { DetachEvent(I_OnPostOperLogout); }
ModResult	Module::OnPreChangeConnectClass(LocalUser*, const std::shared_ptr<ConnectClass>&, std::optional<Numeric::Numeric>&) { DetachEvent(I_OnPreChangeConnectClass); return MOD_RES_PASSTHRU; }
void		Module::OnChangeConnectClass(LocalUser*, const std::shared_ptr<ConnectClass>&, bool) { DetachEvent(I_OnChangeConnectClass); }
void		Module::OnPostChangeConnectClass(LocalUser*, bool) { DetachEvent(I_OnPostChangeConnectClass); }

ServiceProvider::ServiceProvider(Module* Creator, const std::string& Name, ServiceType Type)
	: creator(Creator)
	, name(Name)
	, service(Type)
{
	if ((ServerInstance) && (ServerInstance->Modules.NewServices))
		ServerInstance->Modules.NewServices->push_back(this);
}

void ServiceProvider::DisableAutoRegister()
{
	if ((ServerInstance) && (ServerInstance->Modules.NewServices))
		stdalgo::erase(*ServerInstance->Modules.NewServices, this);
}

const char* ServiceProvider::GetTypeString() const
{
	switch (service)
	{
		case SERVICE_COMMAND:
			return "command";
		case SERVICE_MODE:
			return "mode";
		case SERVICE_METADATA:
			return "metadata";
		case SERVICE_IOHOOK:
			return "iohook";
		case SERVICE_DATA:
			return "data service";
		case SERVICE_CUSTOM:
			return "module service";
	}
	return "unknown service";
}

bool ModuleManager::Attach(Implementation i, Module* mod)
{
	if (stdalgo::isin(EventHandlers[i], mod))
		return false;

	EventHandlers[i].push_back(mod);
	return true;
}

bool ModuleManager::Detach(Implementation i, Module* mod)
{
	return stdalgo::erase(EventHandlers[i], mod);
}

void ModuleManager::Attach(const Implementation* i, Module* mod, size_t sz)
{
	for (size_t n = 0; n < sz; ++n)
		Attach(i[n], mod);
}

void ModuleManager::Detach(const Implementation* i, Module* mod, size_t sz)
{
	for (size_t n = 0; n < sz; ++n)
		Detach(i[n], mod);
}

void ModuleManager::AttachAll(Module* mod)
{
	for (size_t i = 0; i != I_END; ++i)
		Attach(static_cast<Implementation>(i), mod);
}

void ModuleManager::DetachAll(Module* mod)
{
	for (size_t n = 0; n != I_END; ++n)
		Detach(static_cast<Implementation>(n), mod);
}

void ModuleManager::SetPriority(Module* mod, Priority s)
{
	for (size_t n = 0; n != I_END; ++n)
		SetPriority(mod, static_cast<Implementation>(n), s);
}

bool ModuleManager::SetPriority(Module* mod, Implementation i, Priority s, Module* which)
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
		case PRIORITY_LAST:
		{
			if (prioritizationState != PRIO_STATE_FIRST)
				return true;

			swap_pos = 0;
			break;
		}

		case PRIORITY_FIRST:
		{
			if (prioritizationState != PRIO_STATE_FIRST)
				return true;

			swap_pos = EventHandlers[i].size() - 1;
			break;
		}

		case PRIORITY_BEFORE:
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
		case PRIORITY_AFTER:
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

bool ModuleManager::CanUnload(Module* mod)
{
	std::map<std::string, Module*>::iterator modfind = Modules.find(mod->ModuleFile);

	if ((modfind == Modules.end()) || (modfind->second != mod) || (mod->dying))
	{
		LastModuleError = "Module " + mod->ModuleFile + " is not loaded, cannot unload it!";
		ServerInstance->Logs.Critical("MODULE", LastModuleError);
		return false;
	}

	mod->dying = true;
	return true;
}

void ModuleManager::UnregisterModes(Module* mod, ModeType modetype)
{
	const ModeParser::ModeHandlerMap& modes = ServerInstance->Modes.GetModes(modetype);
	for (ModeParser::ModeHandlerMap::const_iterator i = modes.begin(); i != modes.end(); )
	{
		ModeHandler* const mh = i->second;
		++i;
		if (mh->creator == mod)
			this->DelService(*mh);
	}
}

void ModuleManager::DoSafeUnload(Module* mod)
{
	// First, notify all modules that a module is about to be unloaded, so in case
	// they pass execution to the soon to be unloaded module, it will happen now,
	// i.e. before we unregister the services of the module being unloaded
	FOREACH_MOD(OnUnloadModule, (mod));

	std::map<std::string, Module*>::iterator modfind = Modules.find(mod->ModuleFile);

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
		mod->OnCleanup(ExtensionType::CHANNEL, chan);
		chan->UnhookExtensions(items);
		for (const auto& [_, memb] : chan->GetUsers())
		{
			mod->OnCleanup(ExtensionType::MEMBERSHIP, memb);
			memb->UnhookExtensions(items);
		}
	}

	const UserMap& users = ServerInstance->Users.GetUsers();
	for (UserMap::const_iterator u = users.begin(); u != users.end(); )
	{
		User* user = u->second;
		// The module may quit the user (e.g. TLS mod unloading) and that will remove it from the container
		++u;
		mod->OnCleanup(ExtensionType::USER, user);
		user->UnhookExtensions(items);
	}

	for (DataProviderMap::iterator i = DataProviders.begin(); i != DataProviders.end(); )
	{
		DataProviderMap::iterator curr = i++;
		if (curr->second->creator == mod)
		{
			DataProviders.erase(curr);
			FOREACH_MOD(OnServiceDel, (*curr->second));
		}
	}

	dynamic_reference_base::reset_all();

	DetachAll(mod);

	Modules.erase(modfind);
	ServerInstance->GlobalCulls.AddItem(mod);

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
		std::map<std::string, Module*>::iterator i = Modules.begin();
		while (i != Modules.end())
		{
			std::map<std::string, Module*>::iterator me = i++;
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
		Module* const mod;
		UnloadAction(Module* m)
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

bool ModuleManager::Unload(Module* mod)
{
	if (!CanUnload(mod))
		return false;
	ServerInstance->AtomicActions.AddAction(new UnloadAction(mod));
	return true;
}

void ModuleManager::LoadAll()
{
	std::map<std::string, ServiceList> servicemap;
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
			fmt::println("");
			fmt::println("[{}] {}", fmt::styled("*", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)), LastError());
			fmt::println("");
			ServerInstance->Exit(EXIT_FAILURE);
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
			fmt::println("");
			fmt::println("[{}] {}", fmt::styled("*", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)), LastModuleError);
			fmt::println("");
			ServerInstance->Exit(EXIT_FAILURE);
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
			fmt::println("");
			fmt::println("[{}] {}", fmt::styled("*", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)), LastModuleError);
			fmt::println("");
			ServerInstance->Exit(EXIT_FAILURE);
		}
	}

	if (!PrioritizeHooks())
		ServerInstance->Exit(EXIT_FAILURE);
}

std::string& ModuleManager::LastError()
{
	return LastModuleError;
}

void ModuleManager::AddServices(const ServiceList& list)
{
	for (auto* service : list)
		AddService(*service);
}

void ModuleManager::AddService(ServiceProvider& item)
{
	ServerInstance->Logs.Debug("SERVICE", "Adding {} {} provided by {}", item.name,
		item.GetTypeString(), item.creator ? item.creator->ModuleFile : "the core");
	switch (item.service)
	{
		case SERVICE_DATA:
		case SERVICE_IOHOOK:
		{
			if ((!item.name.compare(0, 5, "mode/", 5)) || (!item.name.compare(0, 6, "umode/", 6)))
				throw ModuleException(item.creator, "The \"mode/\" and the \"umode\" service name prefixes are reserved.");

			DataProviders.emplace(item.name, &item);
			std::string::size_type slash = item.name.find('/');
			if (slash != std::string::npos)
			{
				// Also register foo/bar as foo.
				DataProviders.emplace(item.name.substr(0, slash), &item);
			}

			dynamic_reference_base::reset_all();
			break;
		}
		default:
			item.RegisterService();
	}

	FOREACH_MOD(OnServiceAdd, (item));
}

void ModuleManager::DelService(ServiceProvider& item)
{
	ServerInstance->Logs.Debug("SERVICE", "Deleting {} {} provided by {}", item.name,
		item.GetTypeString(), item.creator ? item.creator->ModuleFile : "the core");
	switch (item.service)
	{
		case SERVICE_MODE:
			if (!ServerInstance->Modes.DelMode(static_cast<ModeHandler*>(&item)))
				throw ModuleException(item.creator, "Mode " + std::string(item.name) + " does not exist.");
			[[fallthrough]];
		case SERVICE_DATA:
		case SERVICE_IOHOOK:
		{
			DelReferent(&item);
			break;
		}
		default:
			throw ModuleException(item.creator, "Cannot delete unknown service type");
	}

	FOREACH_MOD(OnServiceDel, (item));
}

ServiceProvider* ModuleManager::FindService(ServiceType type, const std::string& name)
{
	switch (type)
	{
		case SERVICE_DATA:
		case SERVICE_IOHOOK:
		{
			DataProviderMap::iterator i = DataProviders.find(name);
			if (i != DataProviders.end() && i->second->service == type)
				return i->second;
			return nullptr;
		}
		// TODO implement finding of the other types
		default:
			throw CoreException("Cannot find unknown service type");
	}
}

std::string ModuleManager::ExpandModName(const std::string& modname)
{
	const static size_t extlen = strlen(DLL_EXTENSION);
	std::string fullname;
	if (modname.compare(0, 5, "core_") != 0 && modname.compare(0, 2, "m_") != 0)
		fullname.append("m_");
	fullname.append(modname);
	if (modname.length() < extlen || modname.compare(modname.size() - extlen, extlen, DLL_EXTENSION) != 0)
		fullname.append(DLL_EXTENSION);
	return fullname;
}

std::string ModuleManager::ShrinkModName(const std::string& modname)
{
	const static size_t extlen = strlen(DLL_EXTENSION);
	size_t startpos = modname.compare(0, 2, "m_", 2) ? 0 : 2;
	size_t endpos = modname.length() < extlen || modname.compare(modname.length() - extlen, extlen, DLL_EXTENSION, extlen) ? 0 : extlen;
	return modname.substr(startpos, modname.length() - endpos - startpos);
}

dynamic_reference_base::dynamic_reference_base(Module* Creator, const std::string& Name)
	: name(Name)
	, creator(Creator)
{
	if (!dynrefs)
		dynrefs = new insp::intrusive_list<dynamic_reference_base>;
	dynrefs->push_front(this);

	// Resolve unless there is no ModuleManager (part of class InspIRCd)
	if (ServerInstance)
		resolve();
}

dynamic_reference_base::~dynamic_reference_base()
{
	dynrefs->erase(this);
	if (dynrefs->empty())
		stdalgo::delete_zero(dynrefs);
}

void dynamic_reference_base::SetProvider(const std::string& newname)
{
	name = newname;
	resolve();
}

void dynamic_reference_base::ClearProvider()
{
	name.clear();
	value = nullptr;
}

void dynamic_reference_base::resolve()
{
	// Because find() may return any element with a matching key in case count(key) > 1 use lower_bound()
	// to ensure a dynref with the same name as another one resolves to the same object
	ModuleManager::DataProviderMap::iterator i = ServerInstance->Modules.DataProviders.lower_bound(name);
	if ((i != ServerInstance->Modules.DataProviders.end()) && (i->first == this->name))
	{
		ServiceProvider* newvalue = i->second;
		if (value != newvalue)
		{
			value = newvalue;
			if (hook)
				hook->OnCapture();
		}
	}
	else
		value = nullptr;
}

Module* ModuleManager::Find(const std::string& name)
{
	std::map<std::string, Module*>::const_iterator modfind = Modules.find(ExpandModName(name));

	if (modfind == Modules.end())
		return nullptr;
	else
		return modfind->second;
}

void ModuleManager::AddReferent(const std::string& name, ServiceProvider* service)
{
	DataProviders.emplace(name, service);
	dynamic_reference_base::reset_all();
}

void ModuleManager::DelReferent(ServiceProvider* service)
{
	for (DataProviderMap::iterator i = DataProviders.begin(); i != DataProviders.end(); )
	{
		ServiceProvider* curr = i->second;
		if (curr == service)
			DataProviders.erase(i++);
		else
			++i;
	}
	dynamic_reference_base::reset_all();
}
