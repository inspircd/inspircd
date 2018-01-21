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


#include "inspircd.h"
#include "xline.h"
#include "socket.h"
#include "socketengine.h"
#include "command_parse.h"
#include "dns.h"
#include "exitcodes.h"

#ifndef _WIN32
	#include <dirent.h>
#endif

static std::vector<dynamic_reference_base*>* dynrefs = NULL;

void dynamic_reference_base::reset_all()
{
	if (!dynrefs)
		return;
	for(unsigned int i = 0; i < dynrefs->size(); i++)
		(*dynrefs)[i]->ClearCache();
}

// Version is a simple class for holding a modules version number
Version::Version(const std::string &desc, int flags) : description(desc), Flags(flags)
{
}

Version::Version(const std::string &desc, int flags, const std::string& linkdata)
: description(desc), Flags(flags), link_data(linkdata)
{
}

Request::Request(Module* src, Module* dst, const char* idstr)
: id(idstr), source(src), dest(dst)
{
}

void Request::Send()
{
	if (dest)
		dest->OnRequest(*this);
}

Event::Event(Module* src, const std::string &eventid) : source(src), id(eventid) { }

void Event::Send()
{
	FOREACH_MOD(I_OnEvent,OnEvent(*this));
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

ModResult	Module::OnSendSnotice(char &snomask, std::string &type, const std::string &message) { return MOD_RES_PASSTHRU; }
void		Module::OnUserConnect(LocalUser*) { }
void		Module::OnUserQuit(User*, const std::string&, const std::string&) { }
void		Module::OnUserDisconnect(LocalUser*) { }
void		Module::OnUserJoin(Membership*, bool, bool, CUList&) { }
void		Module::OnPostJoin(Membership*) { }
void		Module::OnUserPart(Membership*, std::string&, CUList&) { }
void		Module::OnPreRehash(User*, const std::string&) { }
void		Module::OnModuleRehash(User*, const std::string&) { }
void		Module::OnRehash(User*) { }
ModResult	Module::OnUserPreJoin(User*, Channel*, const char*, std::string&, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnMode(User*, void*, int, const std::vector<std::string>&, const std::vector<TranslateType>&) { }
void		Module::OnOper(User*, const std::string&) { }
void		Module::OnPostOper(User*, const std::string&, const std::string &) { }
void		Module::OnPostDeoper(User*) { }
void		Module::OnInfo(User*) { }
void		Module::OnWhois(User*, User*) { }
ModResult	Module::OnUserPreInvite(User*, User*, Channel*, time_t) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreMessage(User*, void*, int, std::string&, char, CUList&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreNotice(User*, void*, int, std::string&, char, CUList&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreNick(User*, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnUserPostNick(User*, const std::string&) { }
ModResult	Module::OnPreMode(User*, User*, Channel*, const std::vector<std::string>&) { return MOD_RES_PASSTHRU; }
void		Module::On005Numeric(std::string&) { }
ModResult	Module::OnKill(User*, User*, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnLoadModule(Module*) { }
void		Module::OnUnloadModule(Module*) { }
void		Module::OnBackgroundTimer(time_t) { }
ModResult	Module::OnPreCommand(std::string&, std::vector<std::string>&, LocalUser*, bool, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnPostCommand(const std::string&, const std::vector<std::string>&, LocalUser*, CmdResult, const std::string&) { }
void		Module::OnUserInit(LocalUser*) { }
ModResult	Module::OnCheckReady(LocalUser*) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserRegister(LocalUser*) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreKick(User*, Membership*, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnUserKick(User*, Membership*, const std::string&, CUList&) { }
ModResult	Module::OnRawMode(User*, Channel*, const char, const std::string &, bool, int) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckInvite(User*, Channel*) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckKey(User*, Channel*, const std::string&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckLimit(User*, Channel*) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckChannelBan(User*, Channel*) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckBan(User*, Channel*, const std::string&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnExtBanCheck(User*, Channel*, char) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnStats(char, User*, string_list&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnChangeLocalUserHost(LocalUser*, const std::string&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnChangeLocalUserGECOS(LocalUser*, const std::string&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnPreTopicChange(User*, Channel*, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnEvent(Event&) { }
void		Module::OnRequest(Request&) { }
ModResult	Module::OnPassCompare(Extensible* ex, const std::string &password, const std::string &input, const std::string& hashtype) { return MOD_RES_PASSTHRU; }
void		Module::OnGlobalOper(User*) { }
void		Module::OnPostConnect(User*) { }
ModResult	Module::OnAddBan(User*, Channel*, const std::string &) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnDelBan(User*, Channel*, const std::string &) { return MOD_RES_PASSTHRU; }
void		Module::OnStreamSocketAccept(StreamSocket*, irc::sockets::sockaddrs*, irc::sockets::sockaddrs*) { }
int		Module::OnStreamSocketWrite(StreamSocket*, std::string&) { return -1; }
void		Module::OnStreamSocketClose(StreamSocket*) { }
void		Module::OnStreamSocketConnect(StreamSocket*) { }
int		Module::OnStreamSocketRead(StreamSocket*, std::string&) { return -1; }
void		Module::OnUserMessage(User*, void*, int, const std::string&, char, const CUList&) { }
void		Module::OnUserNotice(User*, void*, int, const std::string&, char, const CUList&) { }
void 		Module::OnRemoteKill(User*, User*, const std::string&, const std::string&) { }
void		Module::OnUserInvite(User*, User*, Channel*, time_t) { }
void		Module::OnPostTopicChange(User*, Channel*, const std::string&) { }
void		Module::OnGetServerDescription(const std::string&, std::string&) { }
void		Module::OnSyncUser(User*, Module*, void*) { }
void		Module::OnSyncChannel(Channel*, Module*, void*) { }
void		Module::OnSyncNetwork(Module*, void*) { }
void		Module::ProtoSendMode(void*, TargetTypeFlags, void*, const std::vector<std::string>&, const std::vector<TranslateType>&) { }
void		Module::OnDecodeMetaData(Extensible*, const std::string&, const std::string&) { }
void		Module::ProtoSendMetaData(void*, Extensible*, const std::string&, const std::string&) { }
void		Module::OnWallops(User*, const std::string&) { }
void		Module::OnChangeHost(User*, const std::string&) { }
void		Module::OnChangeName(User*, const std::string&) { }
void		Module::OnChangeIdent(User*, const std::string&) { }
void		Module::OnAddLine(User*, XLine*) { }
void		Module::OnDelLine(User*, XLine*) { }
void		Module::OnExpireLine(XLine*) { }
void 		Module::OnCleanup(int, void*) { }
ModResult	Module::OnChannelPreDelete(Channel*) { return MOD_RES_PASSTHRU; }
void		Module::OnChannelDelete(Channel*) { }
ModResult	Module::OnSetAway(User*, const std::string &) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnWhoisLine(User*, User*, int&, std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnBuildNeighborList(User*, UserChanList&, std::map<User*,bool>&) { }
void		Module::OnGarbageCollect() { }
ModResult	Module::OnSetConnectClass(LocalUser* user, ConnectClass* myclass) { return MOD_RES_PASSTHRU; }
void 		Module::OnText(User*, void*, int, const std::string&, char, CUList&) { }
void		Module::OnRunTestSuite() { }
void		Module::OnNamesListItem(User*, Membership*, std::string&, std::string&) { }
ModResult	Module::OnNumeric(User*, unsigned int, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnHookIO(StreamSocket*, ListenSocket*) { }
ModResult   Module::OnAcceptConnection(int, ListenSocket*, irc::sockets::sockaddrs*, irc::sockets::sockaddrs*) { return MOD_RES_PASSTHRU; }
void		Module::OnSendWhoLine(User*, const std::vector<std::string>&, User*, std::string&) { }
void		Module::OnSetUserIP(LocalUser*) { }

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
		ServerInstance->Logs->Log("MODULE", DEFAULT, LastModuleError);
		return false;
	}
	if (mod->GetVersion().Flags & VF_STATIC)
	{
		LastModuleError = "Module " + mod->ModuleSourceFile + " not unloadable (marked static)";
		ServerInstance->Logs->Log("MODULE", DEFAULT, LastModuleError);
		return false;
	}

	mod->dying = true;
	return true;
}

void ModuleManager::DoSafeUnload(Module* mod)
{
	std::map<std::string, Module*>::iterator modfind = Modules.find(mod->ModuleSourceFile);

	std::vector<reference<ExtensionItem> > items;
	ServerInstance->Extensions.BeginUnregister(modfind->second, items);
	/* Give the module a chance to tidy out all its metadata */
	for (chan_hash::iterator c = ServerInstance->chanlist->begin(); c != ServerInstance->chanlist->end(); )
	{
		Channel* chan = c->second;
		++c;
		mod->OnCleanup(TYPE_CHANNEL, chan);
		chan->doUnhookExtensions(items);
		const UserMembList* users = chan->GetUsers();
		for(UserMembCIter mi = users->begin(); mi != users->end(); mi++)
			mi->second->doUnhookExtensions(items);
	}
	for (user_hash::iterator u = ServerInstance->Users->clientlist->begin(); u != ServerInstance->Users->clientlist->end(); )
	{
		User* user = u->second;
		// The module may quit the user (e.g. SSL mod unloading) and that will remove it from the container
		++u;
		mod->OnCleanup(TYPE_USER, user);
		user->doUnhookExtensions(items);
	}
	for(char m='A'; m <= 'z'; m++)
	{
		ModeHandler* mh;
		mh = ServerInstance->Modes->FindMode(m, MODETYPE_USER);
		if (mh && mh->creator == mod)
			ServerInstance->Modes->DelMode(mh);
		mh = ServerInstance->Modes->FindMode(m, MODETYPE_CHANNEL);
		if (mh && mh->creator == mod)
			ServerInstance->Modes->DelMode(mh);
	}
	for(std::multimap<std::string, ServiceProvider*>::iterator i = DataProviders.begin(); i != DataProviders.end(); )
	{
		std::multimap<std::string, ServiceProvider*>::iterator curr = i++;
		if (curr->second->creator == mod)
			DataProviders.erase(curr);
	}

	dynamic_reference_base::reset_all();

	/* Tidy up any dangling resolvers */
	ServerInstance->Res->CleanResolvers(mod);

	FOREACH_MOD(I_OnUnloadModule,OnUnloadModule(mod));

	DetachAll(mod);

	Modules.erase(modfind);
	ServerInstance->GlobalCulls.AddItem(mod);

	ServerInstance->Logs->Log("MODULE", DEFAULT,"Module %s unloaded",mod->ModuleSourceFile.c_str());
	this->ModCount--;
	ServerInstance->BuildISupport();
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

CmdResult InspIRCd::CallCommandHandler(const std::string &commandname, const std::vector<std::string>& parameters, User* user)
{
	return this->Parser->CallHandler(commandname, parameters, user);
}

bool InspIRCd::IsValidModuleCommand(const std::string &commandname, int pcnt, User* user)
{
	return this->Parser->IsValidCommand(commandname, pcnt, user);
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
			if (!ServerInstance->Modes->AddMode(static_cast<ModeHandler*>(&item)))
				throw ModuleException("Mode "+std::string(item.name)+" already exists.");
			return;
		case SERVICE_METADATA:
			if (!ServerInstance->Extensions.Register(static_cast<ExtensionItem*>(&item)))
				throw ModuleException("Extension " + std::string(item.name) + " already exists.");
			return;
		case SERVICE_DATA:
		case SERVICE_IOHOOK:
		{
			DataProviders.insert(std::make_pair(item.name, &item));
			std::string::size_type slash = item.name.find('/');
			if (slash != std::string::npos)
			{
				DataProviders.insert(std::make_pair(item.name.substr(0, slash), &item));
				DataProviders.insert(std::make_pair(item.name.substr(slash + 1), &item));
			}
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
			return;
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
	ClearCache();
}

void dynamic_reference_base::lookup()
{
	if (!*this)
		throw ModuleException("Dynamic reference to '" + name + "' failed to resolve");
}

dynamic_reference_base::operator bool()
{
	if (!value)
	{
		std::multimap<std::string, ServiceProvider*>::iterator i = ServerInstance->Modules->DataProviders.find(name);
		if (i != ServerInstance->Modules->DataProviders.end())
			value = static_cast<DataProvider*>(i->second);
	}
	return (value != NULL);
}

void InspIRCd::SendMode(const std::vector<std::string>& parameters, User *user)
{
	this->Modes->Process(parameters, user);
}


void InspIRCd::SendGlobalMode(const std::vector<std::string>& parameters, User *user)
{
	Modes->Process(parameters, user);
	if (!Modes->GetLastParse().empty())
		this->PI->SendMode(parameters[0], Modes->GetLastParseParams(), Modes->GetLastParseTranslate());
}

bool InspIRCd::AddResolver(Resolver* r, bool cached)
{
	if (!cached)
		return this->Res->AddResolverClass(r);
	else
	{
		r->TriggerCachedResult();
		delete r;
		return true;
	}
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

ConfigReader::ConfigReader()
{
	this->error = 0;
	ServerInstance->Logs->Log("MODULE", DEBUG, "ConfigReader is deprecated in 2.0; "
		"use ServerInstance->Config->ConfValue(\"key\") or ->ConfTags(\"key\") instead");
}


ConfigReader::~ConfigReader()
{
}

static ConfigTag* SlowGetTag(const std::string &tag, int index)
{
	ConfigTagList tags = ServerInstance->Config->ConfTags(tag);
	while (tags.first != tags.second)
	{
		if (!index)
			return tags.first->second;
		tags.first++;
		index--;
	}
	return NULL;
}

std::string ConfigReader::ReadValue(const std::string &tag, const std::string &name, const std::string &default_value, int index, bool allow_linefeeds)
{
	std::string result = default_value;
	ConfigTag* conftag = SlowGetTag(tag, index);
	if (!conftag || !conftag->readString(name, result, allow_linefeeds))
	{
		this->error = CONF_VALUE_NOT_FOUND;
	}
	return result;
}

std::string ConfigReader::ReadValue(const std::string &tag, const std::string &name, int index, bool allow_linefeeds)
{
	return ReadValue(tag, name, "", index, allow_linefeeds);
}

bool ConfigReader::ReadFlag(const std::string &tag, const std::string &name, const std::string &default_value, int index)
{
	bool def = (default_value == "yes");
	ConfigTag* conftag = SlowGetTag(tag, index);
	return conftag ? conftag->getBool(name, def) : def;
}

bool ConfigReader::ReadFlag(const std::string &tag, const std::string &name, int index)
{
	return ReadFlag(tag, name, "", index);
}


int ConfigReader::ReadInteger(const std::string &tag, const std::string &name, const std::string &default_value, int index, bool need_positive)
{
	int v = atoi(default_value.c_str());
	ConfigTag* conftag = SlowGetTag(tag, index);
	int result = conftag ? conftag->getInt(name, v) : v;

	if ((need_positive) && (result < 0))
	{
		this->error = CONF_INT_NEGATIVE;
		return 0;
	}

	return result;
}

int ConfigReader::ReadInteger(const std::string &tag, const std::string &name, int index, bool need_positive)
{
	return ReadInteger(tag, name, "", index, need_positive);
}

long ConfigReader::GetError()
{
	long olderr = this->error;
	this->error = 0;
	return olderr;
}

int ConfigReader::Enumerate(const std::string &tag)
{
	ServerInstance->Logs->Log("MODULE", DEBUG, "Module is using ConfigReader::Enumerate on %s; this is slow!",
		tag.c_str());
	int i=0;
	while (SlowGetTag(tag, i)) i++;
	return i;
}

FileReader::FileReader(const std::string &filename)
{
	LoadFile(filename);
}

FileReader::FileReader()
{
}

std::string FileReader::Contents()
{
	std::string x;
	for (file_cache::iterator a = this->fc.begin(); a != this->fc.end(); a++)
	{
		x.append(*a);
		x.append("\r\n");
	}
	return x;
}

unsigned long FileReader::ContentSize()
{
	return this->contentsize;
}

void FileReader::CalcSize()
{
	unsigned long n = 0;
	for (file_cache::iterator a = this->fc.begin(); a != this->fc.end(); a++)
		n += (a->length() + 2);
	this->contentsize = n;
}

void FileReader::LoadFile(const std::string &filename)
{
	std::map<std::string, file_cache>::iterator file = ServerInstance->Config->Files.find(filename);
	if (file != ServerInstance->Config->Files.end())
	{
		this->fc = file->second;
	}
	else
	{
		fc.clear();
		FILE* f = fopen(filename.c_str(), "r");
		if (!f)
			return;
		char linebuf[MAXBUF*10];
		while (fgets(linebuf, sizeof(linebuf), f))
		{
			int len = strlen(linebuf);
			if (len)
				fc.push_back(std::string(linebuf, len - 1));
		}
		fclose(f);
	}
	CalcSize();
}


FileReader::~FileReader()
{
}

bool FileReader::Exists()
{
	return (!(fc.size() == 0));
}

std::string FileReader::GetLine(int x)
{
	if ((x<0) || ((unsigned)x>=fc.size()))
		return "";
	return fc[x];
}

int FileReader::FileSize()
{
	return fc.size();
}
