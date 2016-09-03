/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2003-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2006-2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2007 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2003 randomdan <???@???>
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


#include <iostream>
#include "inspircd.h"
#include "exitcodes.h"

#ifndef _WIN32
	#include <dirent.h>
#endif

static insp::intrusive_list<dynamic_reference_base>* dynrefs = NULL;

void dynamic_reference_base::reset_all()
{
	if (!dynrefs)
		return;
	for (insp::intrusive_list<dynamic_reference_base>::iterator i = dynrefs->begin(); i != dynrefs->end(); ++i)
		(*i)->resolve();
}

// Version is a simple class for holding a modules version number
Version::Version(const std::string &desc, int flags) : description(desc), Flags(flags)
{
}

Version::Version(const std::string &desc, int flags, const std::string& linkdata)
: description(desc), Flags(flags), link_data(linkdata)
{
}

// These declarations define the behavours of the base class Module (which does nothing at all)

Module::Module() { }
CullResult Module::cull()
{
	return classbase::cull();
}
Module::~Module()
{
}

void Module::DetachEvent(Implementation i)
{
	ServerInstance->Modules->Detach(i, this);
}

void		Module::ReadConfig(ConfigStatus& status) { }
ModResult	Module::OnSendSnotice(char &snomask, std::string &type, const std::string &message) { DetachEvent(I_OnSendSnotice); return MOD_RES_PASSTHRU; }
void		Module::OnUserConnect(LocalUser*) { DetachEvent(I_OnUserConnect); }
void		Module::OnUserQuit(User*, const std::string&, const std::string&) { DetachEvent(I_OnUserQuit); }
void		Module::OnUserDisconnect(LocalUser*) { DetachEvent(I_OnUserDisconnect); }
void		Module::OnUserJoin(Membership*, bool, bool, CUList&) { DetachEvent(I_OnUserJoin); }
void		Module::OnPostJoin(Membership*) { DetachEvent(I_OnPostJoin); }
void		Module::OnUserPart(Membership*, std::string&, CUList&) { DetachEvent(I_OnUserPart); }
void		Module::OnPreRehash(User*, const std::string&) { DetachEvent(I_OnPreRehash); }
void		Module::OnModuleRehash(User*, const std::string&) { DetachEvent(I_OnModuleRehash); }
ModResult	Module::OnUserPreJoin(LocalUser*, Channel*, const std::string&, std::string&, const std::string&) { DetachEvent(I_OnUserPreJoin); return MOD_RES_PASSTHRU; }
void		Module::OnMode(User*, User*, Channel*, const Modes::ChangeList&, ModeParser::ModeProcessFlag, const std::string&) { DetachEvent(I_OnMode); }
void		Module::OnOper(User*, const std::string&) { DetachEvent(I_OnOper); }
void		Module::OnPostOper(User*, const std::string&, const std::string &) { DetachEvent(I_OnPostOper); }
void		Module::OnInfo(User*) { DetachEvent(I_OnInfo); }
ModResult	Module::OnUserPreInvite(User*, User*, Channel*, time_t) { DetachEvent(I_OnUserPreInvite); return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreMessage(User*, void*, int, std::string&, char, CUList&, MessageType) { DetachEvent(I_OnUserPreMessage); return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreNick(LocalUser*, const std::string&) { DetachEvent(I_OnUserPreNick); return MOD_RES_PASSTHRU; }
void		Module::OnUserPostNick(User*, const std::string&) { DetachEvent(I_OnUserPostNick); }
ModResult	Module::OnPreMode(User*, User*, Channel*, Modes::ChangeList&) { DetachEvent(I_OnPreMode); return MOD_RES_PASSTHRU; }
void		Module::On005Numeric(std::map<std::string, std::string>&) { DetachEvent(I_On005Numeric); }
ModResult	Module::OnKill(User*, User*, const std::string&) { DetachEvent(I_OnKill); return MOD_RES_PASSTHRU; }
void		Module::OnLoadModule(Module*) { DetachEvent(I_OnLoadModule); }
void		Module::OnUnloadModule(Module*) { DetachEvent(I_OnUnloadModule); }
void		Module::OnBackgroundTimer(time_t) { DetachEvent(I_OnBackgroundTimer); }
ModResult	Module::OnPreCommand(std::string&, std::vector<std::string>&, LocalUser*, bool, const std::string&) { DetachEvent(I_OnPreCommand); return MOD_RES_PASSTHRU; }
void		Module::OnPostCommand(Command*, const std::vector<std::string>&, LocalUser*, CmdResult, const std::string&) { DetachEvent(I_OnPostCommand); }
void		Module::OnUserInit(LocalUser*) { DetachEvent(I_OnUserInit); }
ModResult	Module::OnCheckReady(LocalUser*) { DetachEvent(I_OnCheckReady); return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserRegister(LocalUser*) { DetachEvent(I_OnUserRegister); return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreKick(User*, Membership*, const std::string&) { DetachEvent(I_OnUserPreKick); return MOD_RES_PASSTHRU; }
void		Module::OnUserKick(User*, Membership*, const std::string&, CUList&) { DetachEvent(I_OnUserKick); }
ModResult	Module::OnRawMode(User*, Channel*, ModeHandler*, const std::string&, bool) { DetachEvent(I_OnRawMode); return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckInvite(User*, Channel*) { DetachEvent(I_OnCheckInvite); return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckKey(User*, Channel*, const std::string&) { DetachEvent(I_OnCheckKey); return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckLimit(User*, Channel*) { DetachEvent(I_OnCheckLimit); return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckChannelBan(User*, Channel*) { DetachEvent(I_OnCheckChannelBan); return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckBan(User*, Channel*, const std::string&) { DetachEvent(I_OnCheckBan); return MOD_RES_PASSTHRU; }
ModResult	Module::OnExtBanCheck(User*, Channel*, char) { DetachEvent(I_OnExtBanCheck); return MOD_RES_PASSTHRU; }
ModResult	Module::OnStats(Stats::Context&) { DetachEvent(I_OnStats); return MOD_RES_PASSTHRU; }
ModResult	Module::OnChangeLocalUserHost(LocalUser*, const std::string&) { DetachEvent(I_OnChangeLocalUserHost); return MOD_RES_PASSTHRU; }
ModResult	Module::OnChangeLocalUserGECOS(LocalUser*, const std::string&) { DetachEvent(I_OnChangeLocalUserGECOS); return MOD_RES_PASSTHRU; }
ModResult	Module::OnPreTopicChange(User*, Channel*, const std::string&) { DetachEvent(I_OnPreTopicChange); return MOD_RES_PASSTHRU; }
ModResult	Module::OnPassCompare(Extensible* ex, const std::string &password, const std::string &input, const std::string& hashtype) { DetachEvent(I_OnPassCompare); return MOD_RES_PASSTHRU; }
void		Module::OnPostConnect(User*) { DetachEvent(I_OnPostConnect); }
void		Module::OnUserMessage(User*, void*, int, const std::string&, char, const CUList&, MessageType) { DetachEvent(I_OnUserMessage); }
void		Module::OnUserInvite(User*, User*, Channel*, time_t, unsigned int, CUList&) { DetachEvent(I_OnUserInvite); }
void		Module::OnPostTopicChange(User*, Channel*, const std::string&) { DetachEvent(I_OnPostTopicChange); }
void		Module::OnSyncUser(User*, ProtocolInterface::Server&) { DetachEvent(I_OnSyncUser); }
void		Module::OnSyncChannel(Channel*, ProtocolInterface::Server&) { DetachEvent(I_OnSyncChannel); }
void		Module::OnSyncNetwork(ProtocolInterface::Server&) { DetachEvent(I_OnSyncNetwork); }
void		Module::OnDecodeMetaData(Extensible*, const std::string&, const std::string&) { DetachEvent(I_OnDecodeMetaData); }
void		Module::OnChangeHost(User*, const std::string&) { DetachEvent(I_OnChangeHost); }
void		Module::OnChangeName(User*, const std::string&) { DetachEvent(I_OnChangeName); }
void		Module::OnChangeIdent(User*, const std::string&) { DetachEvent(I_OnChangeIdent); }
void		Module::OnAddLine(User*, XLine*) { DetachEvent(I_OnAddLine); }
void		Module::OnDelLine(User*, XLine*) { DetachEvent(I_OnDelLine); }
void		Module::OnExpireLine(XLine*) { DetachEvent(I_OnExpireLine); }
void 		Module::OnCleanup(int, void*) { }
ModResult	Module::OnChannelPreDelete(Channel*) { DetachEvent(I_OnChannelPreDelete); return MOD_RES_PASSTHRU; }
void		Module::OnChannelDelete(Channel*) { DetachEvent(I_OnChannelDelete); }
ModResult	Module::OnSetAway(User*, const std::string &) { DetachEvent(I_OnSetAway); return MOD_RES_PASSTHRU; }
void		Module::OnBuildNeighborList(User*, IncludeChanList&, std::map<User*,bool>&) { DetachEvent(I_OnBuildNeighborList); }
void		Module::OnGarbageCollect() { DetachEvent(I_OnGarbageCollect); }
ModResult	Module::OnSetConnectClass(LocalUser* user, ConnectClass* myclass) { DetachEvent(I_OnSetConnectClass); return MOD_RES_PASSTHRU; }
void 		Module::OnText(User*, void*, int, const std::string&, char, CUList&) { DetachEvent(I_OnText); }
ModResult	Module::OnNamesListItem(User*, Membership*, std::string&, std::string&) { DetachEvent(I_OnNamesListItem); return MOD_RES_PASSTHRU; }
ModResult	Module::OnNumeric(User*, const Numeric::Numeric&) { DetachEvent(I_OnNumeric); return MOD_RES_PASSTHRU; }
ModResult   Module::OnAcceptConnection(int, ListenSocket*, irc::sockets::sockaddrs*, irc::sockets::sockaddrs*) { DetachEvent(I_OnAcceptConnection); return MOD_RES_PASSTHRU; }
ModResult	Module::OnSendWhoLine(User*, const std::vector<std::string>&, User*, Membership*, Numeric::Numeric&) { DetachEvent(I_OnSendWhoLine); return MOD_RES_PASSTHRU; }
void		Module::OnSetUserIP(LocalUser*) { DetachEvent(I_OnSetUserIP); }

#ifdef INSPIRCD_ENABLE_TESTSUITE
void		Module::OnRunTestSuite() { }
#endif

ServiceProvider::ServiceProvider(Module* Creator, const std::string& Name, ServiceType Type)
	: creator(Creator), name(Name), service(Type)
{
	if ((ServerInstance) && (ServerInstance->Modules->NewServices))
		ServerInstance->Modules->NewServices->push_back(this);
}

void ServiceProvider::DisableAutoRegister()
{
	if ((ServerInstance) && (ServerInstance->Modules->NewServices))
		stdalgo::erase(*ServerInstance->Modules->NewServices, this);
}

ModuleManager::ModuleManager()
{
}

ModuleManager::~ModuleManager()
{
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

void ModuleManager::Attach(Implementation* i, Module* mod, size_t sz)
{
	for (size_t n = 0; n < sz; ++n)
		Attach(i[n], mod);
}

void ModuleManager::AttachAll(Module* mod)
{
	for (size_t i = 0; i != I_END; ++i)
		Attach((Implementation)i, mod);
}

void ModuleManager::DetachAll(Module* mod)
{
	for (size_t n = 0; n != I_END; ++n)
		Detach((Implementation)n, mod);
}

void ModuleManager::SetPriority(Module* mod, Priority s)
{
	for (size_t n = 0; n != I_END; ++n)
		SetPriority(mod, (Implementation)n, s);
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
	 * theyre not attached to.
	 */
	return false;

found_src:
	// The modules registered for a hook are called in reverse order (to allow for easier removal
	// of list entries while looping), meaning that the Priority given to us has the exact opposite effect
	// on the list, e.g.: PRIORITY_BEFORE will actually put 'mod' after 'which', etc.
	size_t swap_pos = my_pos;
	switch (s)
	{
		case PRIORITY_LAST:
			if (prioritizationState != PRIO_STATE_FIRST)
				return true;
			else
				swap_pos = 0;
			break;
		case PRIORITY_FIRST:
			if (prioritizationState != PRIO_STATE_FIRST)
				return true;
			else
				swap_pos = EventHandlers[i].size() - 1;
			break;
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
	}

swap_now:
	/* Do we need to swap? */
	if (swap_pos != my_pos)
	{
		// We are going to change positions; we'll need to run again to verify all requirements
		if (prioritizationState == PRIO_STATE_LAST)
			prioritizationState = PRIO_STATE_AGAIN;
		/* Suggestion from Phoenix, "shuffle" the modules to better retain call order */
		int incrmnt = 1;

		if (my_pos > swap_pos)
			incrmnt = -1;

		for (unsigned int j = my_pos; j != swap_pos; j += incrmnt)
		{
			if ((j + incrmnt > EventHandlers[i].size() - 1) || ((incrmnt == -1) && (j == 0)))
				continue;

			std::swap(EventHandlers[i][j], EventHandlers[i][j+incrmnt]);
		}
	}

	return true;
}

bool ModuleManager::PrioritizeHooks()
{
	/* We give every module a chance to re-prioritize when we introduce a new one,
	 * not just the one thats loading, as the new module could affect the preference
	 * of others
	 */
	for (int tries = 0; tries < 20; tries++)
	{
		prioritizationState = tries > 0 ? PRIO_STATE_LAST : PRIO_STATE_FIRST;
		for (std::map<std::string, Module*>::iterator n = Modules.begin(); n != Modules.end(); ++n)
			n->second->Prioritize();

		if (prioritizationState == PRIO_STATE_LAST)
			break;
		if (tries == 19)
		{
			ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, "Hook priority dependency loop detected");
			return false;
		}
	}
	return true;
}

bool ModuleManager::CanUnload(Module* mod)
{
	std::map<std::string, Module*>::iterator modfind = Modules.find(mod->ModuleSourceFile);

	if ((modfind == Modules.end()) || (modfind->second != mod) || (mod->dying))
	{
		LastModuleError = "Module " + mod->ModuleSourceFile + " is not loaded, cannot unload it!";
		ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, LastModuleError);
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

	std::map<std::string, Module*>::iterator modfind = Modules.find(mod->ModuleSourceFile);

	// Unregister modes before extensions because modes may require their extension to show the mode being unset
	UnregisterModes(mod, MODETYPE_USER);
	UnregisterModes(mod, MODETYPE_CHANNEL);

	std::vector<reference<ExtensionItem> > items;
	ServerInstance->Extensions.BeginUnregister(modfind->second, items);
	/* Give the module a chance to tidy out all its metadata */
	const chan_hash& chans = ServerInstance->GetChans();
	for (chan_hash::const_iterator c = chans.begin(); c != chans.end(); )
	{
		Channel* chan = c->second;
		++c;
		mod->OnCleanup(TYPE_CHANNEL, chan);
		chan->doUnhookExtensions(items);
		const Channel::MemberMap& users = chan->GetUsers();
		for (Channel::MemberMap::const_iterator mi = users.begin(); mi != users.end(); ++mi)
			mi->second->doUnhookExtensions(items);
	}

	const user_hash& users = ServerInstance->Users->GetUsers();
	for (user_hash::const_iterator u = users.begin(); u != users.end(); )
	{
		User* user = u->second;
		// The module may quit the user (e.g. SSL mod unloading) and that will remove it from the container
		++u;
		mod->OnCleanup(TYPE_USER, user);
		user->doUnhookExtensions(items);
	}

	for(std::multimap<std::string, ServiceProvider*>::iterator i = DataProviders.begin(); i != DataProviders.end(); )
	{
		std::multimap<std::string, ServiceProvider*>::iterator curr = i++;
		if (curr->second->creator == mod)
			DataProviders.erase(curr);
	}

	dynamic_reference_base::reset_all();

	DetachAll(mod);

	Modules.erase(modfind);
	ServerInstance->GlobalCulls.AddItem(mod);

	ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, "Module %s unloaded",mod->ModuleSourceFile.c_str());
	ServerInstance->ISupport.Build();
}

void ModuleManager::UnloadAll()
{
	/* We do this more than once, so that any service providers get a
	 * chance to be unhooked by the modules using them, but then get
	 * a chance to be removed themsleves.
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
	struct UnloadAction : public HandlerBase0<void>
	{
		Module* const mod;
		UnloadAction(Module* m) : mod(m) {}
		void Call()
		{
			DLLManager* dll = mod->ModuleDLLManager;
			ServerInstance->Modules->DoSafeUnload(mod);
			ServerInstance->GlobalCulls.Apply();
			// In pure static mode this is always NULL
			delete dll;
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

	ConfigTagList tags = ServerInstance->Config->ConfTags("module");
	for (ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string name = tag->getString("name");
		this->NewServices = &servicemap[ExpandModName(name)];
		std::cout << "[" << con_green << "*" << con_reset << "] Loading module:\t" << con_green << name << con_reset << std::endl;

		if (!this->Load(name, true))
		{
			ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, this->LastError());
			std::cout << std::endl << "[" << con_red << "*" << con_reset << "] " << this->LastError() << std::endl << std::endl;
			ServerInstance->Exit(EXIT_STATUS_MODULE);
		}
	}

	ConfigStatus confstatus;

	for (ModuleMap::const_iterator i = Modules.begin(); i != Modules.end(); ++i)
	{
		Module* mod = i->second;
		try
		{
			ServerInstance->Logs->Log("MODULE", LOG_DEBUG, "Initializing %s", i->first.c_str());
			AttachAll(mod);
			AddServices(servicemap[i->first]);
			mod->init();
			mod->ReadConfig(confstatus);
		}
		catch (CoreException& modexcept)
		{
			LastModuleError = "Unable to initialize " + mod->ModuleSourceFile + ": " + modexcept.GetReason();
			ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, LastModuleError);
			std::cout << std::endl << "[" << con_red << "*" << con_reset << "] " << LastModuleError << std::endl << std::endl;
			ServerInstance->Exit(EXIT_STATUS_MODULE);
		}
	}

	this->NewServices = NULL;

	if (!PrioritizeHooks())
		ServerInstance->Exit(EXIT_STATUS_MODULE);
}

std::string& ModuleManager::LastError()
{
	return LastModuleError;
}

void ModuleManager::AddServices(const ServiceList& list)
{
	for (ServiceList::const_iterator i = list.begin(); i != list.end(); ++i)
	{
		ServiceProvider& s = **i;
		AddService(s);
	}
}

void ModuleManager::AddService(ServiceProvider& item)
{
	switch (item.service)
	{
		case SERVICE_DATA:
		case SERVICE_IOHOOK:
		{
			if ((!item.name.compare(0, 5, "mode/", 5)) || (!item.name.compare(0, 6, "umode/", 6)))
				throw ModuleException("The \"mode/\" and the \"umode\" service name prefixes are reserved.");

			DataProviders.insert(std::make_pair(item.name, &item));
			std::string::size_type slash = item.name.find('/');
			if (slash != std::string::npos)
			{
				DataProviders.insert(std::make_pair(item.name.substr(0, slash), &item));
				DataProviders.insert(std::make_pair(item.name.substr(slash + 1), &item));
			}
			dynamic_reference_base::reset_all();
			return;
		}
		default:
			item.RegisterService();
	}
}

void ModuleManager::DelService(ServiceProvider& item)
{
	switch (item.service)
	{
		case SERVICE_MODE:
			if (!ServerInstance->Modes->DelMode(static_cast<ModeHandler*>(&item)))
				throw ModuleException("Mode "+std::string(item.name)+" does not exist.");
			// Fall through
		case SERVICE_DATA:
		case SERVICE_IOHOOK:
		{
			DelReferent(&item);
			return;
		}
		default:
			throw ModuleException("Cannot delete unknown service type");
	}
}

ServiceProvider* ModuleManager::FindService(ServiceType type, const std::string& name)
{
	switch (type)
	{
		case SERVICE_DATA:
		case SERVICE_IOHOOK:
		{
			std::multimap<std::string, ServiceProvider*>::iterator i = DataProviders.find(name);
			if (i != DataProviders.end() && i->second->service == type)
				return i->second;
			return NULL;
		}
		// TODO implement finding of the other types
		default:
			throw ModuleException("Cannot find unknown service type");
	}
}

std::string ModuleManager::ExpandModName(const std::string& modname)
{
	// Transform "callerid" -> "m_callerid.so" unless it already has a ".so" extension,
	// so coremods in the "core_*.so" form aren't changed
	std::string ret = modname;
	if ((modname.length() < 3) || (modname.compare(modname.size() - 3, 3, ".so")))
		ret.insert(0, "m_").append(".so");
	return ret;
}

dynamic_reference_base::dynamic_reference_base(Module* Creator, const std::string& Name)
	: name(Name), hook(NULL), value(NULL), creator(Creator)
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
	{
		delete dynrefs;
		dynrefs = NULL;
	}
}

void dynamic_reference_base::SetProvider(const std::string& newname)
{
	name = newname;
	resolve();
}

void dynamic_reference_base::resolve()
{
	// Because find() may return any element with a matching key in case count(key) > 1 use lower_bound()
	// to ensure a dynref with the same name as another one resolves to the same object
	std::multimap<std::string, ServiceProvider*>::iterator i = ServerInstance->Modules.DataProviders.lower_bound(name);
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
		value = NULL;
}

Module* ModuleManager::Find(const std::string &name)
{
	std::map<std::string, Module*>::const_iterator modfind = Modules.find(ExpandModName(name));

	if (modfind == Modules.end())
		return NULL;
	else
		return modfind->second;
}

void ModuleManager::AddReferent(const std::string& name, ServiceProvider* service)
{
	DataProviders.insert(std::make_pair(name, service));
	dynamic_reference_base::reset_all();
}

void ModuleManager::DelReferent(ServiceProvider* service)
{
	for (std::multimap<std::string, ServiceProvider*>::iterator i = DataProviders.begin(); i != DataProviders.end(); )
	{
		ServiceProvider* curr = i->second;
		if (curr == service)
			DataProviders.erase(i++);
		else
			++i;
	}
	dynamic_reference_base::reset_all();
}
