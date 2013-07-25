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


#include <fstream>
#include "inspircd.h"
#include "xline.h"
#include "socket.h"
#include "socketengine.h"
#include "command_parse.h"
#include "exitcodes.h"

#ifndef _WIN32
	#include <dirent.h>
#endif

static std::vector<dynamic_reference_base*>* dynrefs = NULL;
static bool dynref_init_complete = false;

void dynamic_reference_base::reset_all()
{
	dynref_init_complete = true;
	if (!dynrefs)
		return;
	for(unsigned int i = 0; i < dynrefs->size(); i++)
		(*dynrefs)[i]->resolve();
}

// Version is a simple class for holding a modules version number
Version::Version(const std::string &desc, int flags) : description(desc), Flags(flags)
{
}

Version::Version(const std::string &desc, int flags, const std::string& linkdata)
: description(desc), Flags(flags), link_data(linkdata)
{
}

Event::Event(Module* src, const std::string &eventid) : source(src), id(eventid) { }

void Event::Send()
{
	FOREACH_MOD(OnEvent, (*this));
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

ModResult	Module::OnSendSnotice(char &snomask, std::string &type, const std::string &message) { throw NotImplementedException(); }
void		Module::OnUserConnect(LocalUser*) { throw NotImplementedException(); }
void		Module::OnUserQuit(User*, const std::string&, const std::string&) { throw NotImplementedException(); }
void		Module::OnUserDisconnect(LocalUser*) { throw NotImplementedException(); }
void		Module::OnUserJoin(Membership*, bool, bool, CUList&) { throw NotImplementedException(); }
void		Module::OnPostJoin(Membership*) { throw NotImplementedException(); }
void		Module::OnUserPart(Membership*, std::string&, CUList&) { throw NotImplementedException(); }
void		Module::OnPreRehash(User*, const std::string&) { throw NotImplementedException(); }
void		Module::OnModuleRehash(User*, const std::string&) { throw NotImplementedException(); }
void		Module::OnRehash(User*) { throw NotImplementedException(); }
ModResult	Module::OnUserPreJoin(LocalUser*, Channel*, const std::string&, std::string&, const std::string&) { throw NotImplementedException(); }
void		Module::OnMode(User*, User*, Channel*, const std::vector<std::string>&, const std::vector<TranslateType>&) { throw NotImplementedException(); }
void		Module::OnOper(User*, const std::string&) { throw NotImplementedException(); }
void		Module::OnPostOper(User*, const std::string&, const std::string &) { throw NotImplementedException(); }
void		Module::OnInfo(User*) { throw NotImplementedException(); }
void		Module::OnWhois(User*, User*) { throw NotImplementedException(); }
ModResult	Module::OnUserPreInvite(User*, User*, Channel*, time_t) { throw NotImplementedException(); }
ModResult	Module::OnUserPreMessage(User*, void*, int, std::string&, char, CUList&, MessageType) { throw NotImplementedException(); }
ModResult	Module::OnUserPreNick(User*, const std::string&) { throw NotImplementedException(); }
void		Module::OnUserPostNick(User*, const std::string&) { throw NotImplementedException(); }
ModResult	Module::OnPreMode(User*, User*, Channel*, const std::vector<std::string>&) { throw NotImplementedException(); }
void		Module::On005Numeric(std::map<std::string, std::string>&) { throw NotImplementedException(); }
ModResult	Module::OnKill(User*, User*, const std::string&) { throw NotImplementedException(); }
void		Module::OnLoadModule(Module*) { throw NotImplementedException(); }
void		Module::OnUnloadModule(Module*) { throw NotImplementedException(); }
void		Module::OnBackgroundTimer(time_t) { throw NotImplementedException(); }
ModResult	Module::OnPreCommand(std::string&, std::vector<std::string>&, LocalUser*, bool, const std::string&) { throw NotImplementedException(); }
void		Module::OnPostCommand(Command*, const std::vector<std::string>&, LocalUser*, CmdResult, const std::string&) { throw NotImplementedException(); }
void		Module::OnUserInit(LocalUser*) { throw NotImplementedException(); }
ModResult	Module::OnCheckReady(LocalUser*) { throw NotImplementedException(); }
ModResult	Module::OnUserRegister(LocalUser*) { throw NotImplementedException(); }
ModResult	Module::OnUserPreKick(User*, Membership*, const std::string&) { throw NotImplementedException(); }
void		Module::OnUserKick(User*, Membership*, const std::string&, CUList&) { throw NotImplementedException(); }
ModResult	Module::OnRawMode(User*, Channel*, const char, const std::string &, bool, int) { throw NotImplementedException(); }
ModResult	Module::OnCheckInvite(User*, Channel*) { throw NotImplementedException(); }
ModResult	Module::OnCheckKey(User*, Channel*, const std::string&) { throw NotImplementedException(); }
ModResult	Module::OnCheckLimit(User*, Channel*) { throw NotImplementedException(); }
ModResult	Module::OnCheckChannelBan(User*, Channel*) { throw NotImplementedException(); }
ModResult	Module::OnCheckBan(User*, Channel*, const std::string&) { throw NotImplementedException(); }
ModResult	Module::OnExtBanCheck(User*, Channel*, char) { throw NotImplementedException(); }
ModResult	Module::OnStats(char, User*, string_list&) { throw NotImplementedException(); }
ModResult	Module::OnChangeLocalUserHost(LocalUser*, const std::string&) { throw NotImplementedException(); }
ModResult	Module::OnChangeLocalUserGECOS(LocalUser*, const std::string&) { throw NotImplementedException(); }
ModResult	Module::OnPreTopicChange(User*, Channel*, const std::string&) { throw NotImplementedException(); }
void		Module::OnEvent(Event&) { throw NotImplementedException(); }
ModResult	Module::OnPassCompare(Extensible* ex, const std::string &password, const std::string &input, const std::string& hashtype) { throw NotImplementedException(); }
void		Module::OnGlobalOper(User*) { throw NotImplementedException(); }
void		Module::OnPostConnect(User*) { throw NotImplementedException(); }
void		Module::OnUserMessage(User*, void*, int, const std::string&, char, const CUList&, MessageType) { throw NotImplementedException(); }
void		Module::OnUserInvite(User*, User*, Channel*, time_t) { throw NotImplementedException(); }
void		Module::OnPostTopicChange(User*, Channel*, const std::string&) { throw NotImplementedException(); }
void		Module::OnGetServerDescription(const std::string&, std::string&) { throw NotImplementedException(); }
void		Module::OnSyncUser(User*, Module*, void*) { throw NotImplementedException(); }
void		Module::OnSyncChannel(Channel*, Module*, void*) { throw NotImplementedException(); }
void		Module::OnSyncNetwork(Module*, void*) { throw NotImplementedException(); }
void		Module::ProtoSendMode(void*, TargetTypeFlags, void*, const std::vector<std::string>&, const std::vector<TranslateType>&) { }
void		Module::OnDecodeMetaData(Extensible*, const std::string&, const std::string&) { throw NotImplementedException(); }
void		Module::ProtoSendMetaData(void*, Extensible*, const std::string&, const std::string&) { }
void		Module::OnChangeHost(User*, const std::string&) { throw NotImplementedException(); }
void		Module::OnChangeName(User*, const std::string&) { throw NotImplementedException(); }
void		Module::OnChangeIdent(User*, const std::string&) { throw NotImplementedException(); }
void		Module::OnAddLine(User*, XLine*) { throw NotImplementedException(); }
void		Module::OnDelLine(User*, XLine*) { throw NotImplementedException(); }
void		Module::OnExpireLine(XLine*) { throw NotImplementedException(); }
void 		Module::OnCleanup(int, void*) { }
ModResult	Module::OnChannelPreDelete(Channel*) { throw NotImplementedException(); }
void		Module::OnChannelDelete(Channel*) { throw NotImplementedException(); }
ModResult	Module::OnSetAway(User*, const std::string &) { throw NotImplementedException(); }
ModResult	Module::OnWhoisLine(User*, User*, int&, std::string&) { throw NotImplementedException(); }
void		Module::OnBuildNeighborList(User*, UserChanList&, std::map<User*,bool>&) { throw NotImplementedException(); }
void		Module::OnGarbageCollect() { throw NotImplementedException(); }
ModResult	Module::OnSetConnectClass(LocalUser* user, ConnectClass* myclass) { throw NotImplementedException(); }
void 		Module::OnText(User*, void*, int, const std::string&, char, CUList&) { throw NotImplementedException(); }
void		Module::OnRunTestSuite() { throw NotImplementedException(); }
void		Module::OnNamesListItem(User*, Membership*, std::string&, std::string&) { throw NotImplementedException(); }
ModResult	Module::OnNumeric(User*, unsigned int, const std::string&) { throw NotImplementedException(); }
void		Module::OnHookIO(StreamSocket*, ListenSocket*) { throw NotImplementedException(); }
ModResult   Module::OnAcceptConnection(int, ListenSocket*, irc::sockets::sockaddrs*, irc::sockets::sockaddrs*) { throw NotImplementedException(); }
void		Module::OnSendWhoLine(User*, const std::vector<std::string>&, User*, std::string&) { throw NotImplementedException(); }
void		Module::OnSetUserIP(LocalUser*) { throw NotImplementedException(); }

ModuleManager::ModuleManager() : ModCount(0)
{
}

ModuleManager::~ModuleManager()
{
}

bool ModuleManager::Attach(Implementation i, Module* mod)
{
	if (std::find(EventHandlers[i].begin(), EventHandlers[i].end(), mod) != EventHandlers[i].end())
		return false;

	EventHandlers[i].push_back(mod);
	return true;
}

bool ModuleManager::Detach(Implementation i, Module* mod)
{
	EventHandlerIter x = std::find(EventHandlers[i].begin(), EventHandlers[i].end(), mod);

	if (x == EventHandlers[i].end())
		return false;

	EventHandlers[i].erase(x);
	return true;
}

void ModuleManager::Attach(Implementation* i, Module* mod, size_t sz)
{
	for (size_t n = 0; n < sz; ++n)
		Attach(i[n], mod);
}

void ModuleManager::DetachAll(Module* mod)
{
	for (size_t n = I_BEGIN + 1; n != I_END; ++n)
		Detach((Implementation)n, mod);
}

bool ModuleManager::SetPriority(Module* mod, Priority s)
{
	for (size_t n = I_BEGIN + 1; n != I_END; ++n)
		SetPriority(mod, (Implementation)n, s);

	return true;
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
	size_t swap_pos = my_pos;
	switch (s)
	{
		case PRIORITY_FIRST:
			if (prioritizationState != PRIO_STATE_FIRST)
				return true;
			else
				swap_pos = 0;
			break;
		case PRIORITY_LAST:
			if (prioritizationState != PRIO_STATE_FIRST)
				return true;
			else
				swap_pos = EventHandlers[i].size() - 1;
			break;
		case PRIORITY_AFTER:
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
		case PRIORITY_BEFORE:
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

bool ModuleManager::CanUnload(Module* mod)
{
	std::map<std::string, Module*>::iterator modfind = Modules.find(mod->ModuleSourceFile);

	if ((modfind == Modules.end()) || (modfind->second != mod) || (mod->dying))
	{
		LastModuleError = "Module " + mod->ModuleSourceFile + " is not loaded, cannot unload it!";
		ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, LastModuleError);
		return false;
	}
	if (mod->GetVersion().Flags & VF_STATIC)
	{
		LastModuleError = "Module " + mod->ModuleSourceFile + " not unloadable (marked static)";
		ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, LastModuleError);
		return false;
	}

	mod->dying = true;
	return true;
}

void ModuleManager::DoSafeUnload(Module* mod)
{
	// First, notify all modules that a module is about to be unloaded, so in case
	// they pass execution to the soon to be unloaded module, it will happen now,
	// i.e. before we unregister the services of the module being unloaded
	FOREACH_MOD(OnUnloadModule, (mod));

	std::map<std::string, Module*>::iterator modfind = Modules.find(mod->ModuleSourceFile);

	std::vector<reference<ExtensionItem> > items;
	ServerInstance->Extensions.BeginUnregister(modfind->second, items);
	/* Give the module a chance to tidy out all its metadata */
	for (chan_hash::iterator c = ServerInstance->chanlist->begin(); c != ServerInstance->chanlist->end(); c++)
	{
		mod->OnCleanup(TYPE_CHANNEL,c->second);
		c->second->doUnhookExtensions(items);
		const UserMembList* users = c->second->GetUsers();
		for(UserMembCIter mi = users->begin(); mi != users->end(); mi++)
			mi->second->doUnhookExtensions(items);
	}
	for (user_hash::iterator u = ServerInstance->Users->clientlist->begin(); u != ServerInstance->Users->clientlist->end(); u++)
	{
		mod->OnCleanup(TYPE_USER,u->second);
		u->second->doUnhookExtensions(items);
	}
	for(char m='A'; m <= 'z'; m++)
	{
		ModeHandler* mh;
		mh = ServerInstance->Modes->FindMode(m, MODETYPE_USER);
		if (mh && mh->creator == mod)
			this->DelService(*mh);
		mh = ServerInstance->Modes->FindMode(m, MODETYPE_CHANNEL);
		if (mh && mh->creator == mod)
			this->DelService(*mh);
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
	this->ModCount--;
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

std::string& ModuleManager::LastError()
{
	return LastModuleError;
}

void ModuleManager::AddService(ServiceProvider& item)
{
	switch (item.service)
	{
		case SERVICE_COMMAND:
			if (!ServerInstance->Parser->AddCommand(static_cast<Command*>(&item)))
				throw ModuleException("Command "+std::string(item.name)+" already exists.");
			return;
		case SERVICE_MODE:
		{
			ModeHandler* mh = static_cast<ModeHandler*>(&item);
			if (!ServerInstance->Modes->AddMode(mh))
				throw ModuleException("Mode "+std::string(item.name)+" already exists.");
			DataProviders.insert(std::make_pair((mh->GetModeType() == MODETYPE_CHANNEL ? "mode/" : "umode/") + item.name, &item));
			dynamic_reference_base::reset_all();
			return;
		}
		case SERVICE_METADATA:
			if (!ServerInstance->Extensions.Register(static_cast<ExtensionItem*>(&item)))
				throw ModuleException("Extension " + std::string(item.name) + " already exists.");
			return;
		case SERVICE_DATA:
		case SERVICE_IOHOOK:
		{
			if ((item.name.substr(0, 5) == "mode/") || (item.name.substr(0, 6) == "umode/"))
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
			throw ModuleException("Cannot add unknown service type");
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
			for(std::multimap<std::string, ServiceProvider*>::iterator i = DataProviders.begin(); i != DataProviders.end(); )
			{
				std::multimap<std::string, ServiceProvider*>::iterator curr = i++;
				if (curr->second == &item)
					DataProviders.erase(curr);
			}
			dynamic_reference_base::reset_all();
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

dynamic_reference_base::dynamic_reference_base(Module* Creator, const std::string& Name)
	: name(Name), value(NULL), creator(Creator)
{
	if (!dynrefs)
		dynrefs = new std::vector<dynamic_reference_base*>;
	dynrefs->push_back(this);
	if (dynref_init_complete)
		resolve();
}

dynamic_reference_base::~dynamic_reference_base()
{
	for(unsigned int i = 0; i < dynrefs->size(); i++)
	{
		if (dynrefs->at(i) == this)
		{
			unsigned int last = dynrefs->size() - 1;
			if (i != last)
				dynrefs->at(i) = dynrefs->at(last);
			dynrefs->erase(dynrefs->begin() + last);
			if (dynrefs->empty())
			{
				delete dynrefs;
				dynrefs = NULL;
			}
			return;
		}
	}
}

void dynamic_reference_base::SetProvider(const std::string& newname)
{
	name = newname;
	resolve();
}

void dynamic_reference_base::resolve()
{
	std::multimap<std::string, ServiceProvider*>::iterator i = ServerInstance->Modules->DataProviders.find(name);
	if (i != ServerInstance->Modules->DataProviders.end())
		value = static_cast<DataProvider*>(i->second);
	else
		value = NULL;
}

Module* ModuleManager::Find(const std::string &name)
{
	std::map<std::string, Module*>::iterator modfind = Modules.find(name);

	if (modfind == Modules.end())
		return NULL;
	else
		return modfind->second;
}

const std::vector<std::string> ModuleManager::GetAllModuleNames(int filter)
{
	std::vector<std::string> retval;
	for (std::map<std::string, Module*>::iterator x = Modules.begin(); x != Modules.end(); ++x)
		if (!filter || (x->second->GetVersion().Flags & filter))
			retval.push_back(x->first);
	return retval;
}

FileReader::FileReader(const std::string& filename)
{
	Load(filename);
}

void FileReader::Load(const std::string& filename)
{
	// If the file is stored in the file cache then we used that version instead.
	ConfigFileCache::iterator it = ServerInstance->Config->Files.find(filename);
	if (it != ServerInstance->Config->Files.end())
	{
		this->lines = it->second;
	}
	else
	{
		lines.clear();

		std::ifstream stream(filename.c_str());
		if (!stream.is_open())
			throw CoreException(filename + " does not exist or is not readable!");

		std::string line;
		while (std::getline(stream, line))
		{
			lines.push_back(line);
			totalSize += line.size() + 2;
		}

		stream.close();
	}
}

std::string FileReader::GetString()
{
	std::string buffer;
	for (file_cache::iterator it = this->lines.begin(); it != this->lines.end(); ++it)
	{
		buffer.append(*it);
		buffer.append("\r\n");
	}
	return buffer;
}
