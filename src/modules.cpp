/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"
#include "exitcodes.h"

#ifndef WIN32
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
void		Module::OnModuleRehash(User*, const std::string&) { }
void		Module::ReadConfig(ConfigReadStatus&) { }
void		Module::OnMode(User*, Extensible*, const irc::modestacker&) { }
void		Module::OnOper(User*, const std::string&) { }
void		Module::OnPostOper(User*, const std::string&, const std::string &) { }
void		Module::OnInfo(User*) { }
void		Module::OnWhois(User*, User*) { }
ModResult	Module::OnUserPreMessage(User*, void*, int, std::string&, char, CUList&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreNotice(User*, void*, int, std::string&, char, CUList&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreNick(User*, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnUserPostNick(User*, const std::string&) { }
ModResult	Module::OnPreMode(User*, Extensible*, irc::modestacker&) { return MOD_RES_PASSTHRU; }
void		Module::On005Numeric(std::string&) { }
ModResult	Module::OnKill(User*, User*, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnLoadModule(Module*) { }
void		Module::OnUnloadModule(Module*) { }
void		Module::OnBackgroundTimer(time_t) { }
ModResult	Module::OnPreCommand(std::string&, std::vector<std::string>&, LocalUser*, bool, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnPostCommand(const std::string&, const std::vector<std::string>&, LocalUser*, CmdResult, const std::string&) { }
void		Module::OnUserInit(LocalUser*) { }
ModResult	Module::OnCheckReady(LocalUser*) { return MOD_RES_PASSTHRU; }
void		Module::OnUserRegister(LocalUser*) { }
void		Module::OnUserKick(User*, Membership*, const std::string&, CUList&) { }
ModResult	Module::OnRawMode(User*, Channel*, irc::modechange&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckChannelBan(User*, Channel*) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckBan(User*, Channel*, const std::string&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnExtBanCheck(User*, Channel*, char) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnStats(char, User*, string_list&) { return MOD_RES_PASSTHRU; }
void		Module::OnEvent(Event&) { }
ModResult	Module::OnPassCompare(Extensible* ex, const std::string &password, const std::string &input, const std::string& hashtype) { return MOD_RES_PASSTHRU; }
void		Module::OnPostConnect(User*) { }
void		Module::OnUserMessage(User*, void*, int, const std::string&, char, const CUList&) { }
void		Module::OnUserNotice(User*, void*, int, const std::string&, char, const CUList&) { }
void 		Module::OnRemoteKill(User*, User*, const std::string&, const std::string&) { }
void		Module::OnUserInvite(User*, User*, Channel*, time_t) { }
void		Module::OnPostTopicChange(User*, Channel*, const std::string&) { }
void		Module::OnGetServerDescription(const std::string&, std::string&) { }
void		Module::OnSyncUser(User*, SyncTarget*) { }
void		Module::OnSyncChannel(Channel*, SyncTarget*) { }
void		Module::OnSyncNetwork(SyncTarget*) { }
void		Module::OnDecodeMetaData(Extensible*, const std::string&, const std::string&) { }
void		Module::OnCheckJoin(ChannelPermissionData&) { }
void		Module::OnPermissionCheck(PermissionData&) { }
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
void		Module::OnBuildNeighborList(User*, std::vector<Channel*>&, std::map<User*,bool>&) { }
void		Module::OnGarbageCollect() { }
ModResult	Module::OnSetConnectClass(LocalUser* user, ConnectClass* myclass) { return MOD_RES_PASSTHRU; }
void 		Module::OnText(User*, void*, int, const std::string&, char, CUList&) { }
void		Module::OnNamesListItem(User*, Membership*, std::string&, std::string&) { }
StreamSocket*   Module::OnAcceptConnection(int, ListenSocket*, irc::sockets::sockaddrs*, irc::sockets::sockaddrs*) { return NULL; }
void		Module::OnSendWhoLine(User*, const std::vector<std::string>&, User*, std::string&) { }

ModuleManager::ModuleManager() : ModCount(0)
{
}

ModuleManager::~ModuleManager()
{
}

bool ModuleManager::Attach(Implementation i, Module* mod)
{
	if (Modules.find(mod->ModuleSourceFile) == Modules.end())
		throw CoreException("Module is attaching to hook in constructor; this must be done in init()!");

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
	for (size_t n = I_ModuleInit; n != I_END; ++n)
		Detach((Implementation)n, mod);
}

bool ModuleManager::SetPriority(Module* mod, Priority s)
{
	for (size_t n = I_ModuleInit; n != I_END; ++n)
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
			if (( j + incrmnt > EventHandlers[i].size() - 1) || (j + incrmnt < 0))
				continue;

			std::swap(EventHandlers[i][j], EventHandlers[i][j+incrmnt]);
		}
	}

	return true;
}

bool ModuleManager::CanUnload(Module* mod)
{
	std::map<std::string, Module*>::iterator modfind = Modules.find(mod->ModuleSourceFile);

	if (modfind == Modules.end() || modfind->second != mod)
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
	return true;
}

void ModuleManager::DoSafeUnload(Module* mod, ModuleState* state)
{
	std::map<std::string, Module*>::iterator modfind = Modules.find(mod->ModuleSourceFile);

	std::vector<reference<ExtensionItem> > items;
	ServerInstance->Extensions.BeginUnregister(modfind->second, items);
	std::vector<ModeHandler*> modes;
	/* Give the module a chance to tidy out all its metadata */
	for (chan_hash::iterator c = ServerInstance->chanlist->begin(); c != ServerInstance->chanlist->end(); c++)
	{
		if (state)
		{
			irc::modestacker mlist;
			c->second->ChanModes(mlist, MODELIST_FULL);
			for(std::vector<irc::modechange>::iterator i = mlist.sequence.begin(); i != mlist.sequence.end(); i++)
			{
				ModeHandler* mh = ServerInstance->Modes->FindMode(i->mode);
				if (mh && mh->creator == mod)
					state->channelModes.push_back(RestoreData(c->second->name, mh->name, i->value));
			}
			const Extensible::ExtensibleStore& extlist = c->second->GetExtList();
			for(std::vector<reference<ExtensionItem> >::iterator i = items.begin(); i != items.end(); i++)
			{
				ExtensionItem* item = *i;
				Extensible::ExtensibleStore::const_iterator v = extlist.find(item);
				if (v != extlist.end())
				{
					std::string value = item->serialize(FORMAT_INTERNAL, c->second, v->second);
					if (!value.empty())
						state->channelExt.push_back(RestoreData(c->second->name, item->name, value));
				}
			}
		}
		mod->OnCleanup(TYPE_CHANNEL,c->second);
		c->second->doUnhookExtensions(items);
		const UserMembList* users = c->second->GetUsers();
		for(UserMembCIter mi = users->begin(); mi != users->end(); mi++)
			mi->second->doUnhookExtensions(items);
	}
	for (user_hash::iterator u = ServerInstance->Users->clientlist->begin(); u != ServerInstance->Users->clientlist->end(); u++)
	{
		if (state)
		{
			for(char c='A'; c <= 'z'; c++)
			{
				if (u->second->IsModeSet(c))
				{
					ModeHandler* mh = ServerInstance->Modes->FindMode(c, MODETYPE_USER);
					if (mh && mh->creator == mod)
						state->userModes.push_back(RestoreData(u->second->uuid, mh->name, ""));
				}
			}
			const Extensible::ExtensibleStore& extlist = u->second->GetExtList();
			for(std::vector<reference<ExtensionItem> >::iterator i = items.begin(); i != items.end(); i++)
			{
				ExtensionItem* item = *i;
				Extensible::ExtensibleStore::const_iterator v = extlist.find(item);
				if (v != extlist.end())
				{
					std::string value = item->serialize(FORMAT_INTERNAL, u->second, v->second);
					if (!value.empty())
						state->userExt.push_back(RestoreData(u->second->uuid, item->name, value));
				}
			}
		}
		mod->OnCleanup(TYPE_USER,u->second);
		u->second->doUnhookExtensions(items);
	}

	for(std::multimap<std::string, ServiceProvider*>::iterator i = DataProviders.begin(); i != DataProviders.end(); )
	{
		std::multimap<std::string, ServiceProvider*>::iterator curr = i++;
		if (curr->second->creator == mod)
			DataProviders.erase(curr);
	}
	for(ModeIDIter id; id; id++)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(id);
		if (mh && mh->creator == mod)
			ServerInstance->Modes->DelMode(mh);
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

void ModuleManager::DoModuleLoad(Module* newmod, ModuleState* state)
{
	FOREACH_MOD(I_OnLoadModule,OnLoadModule(newmod));
	/* We give every module a chance to re-prioritize when we introduce a new one,
	 * not just the one thats loading, as the new module could affect the preference
	 * of others
	 */
	for(int tries = 0; tries < 20; tries++)
	{
		prioritizationState = tries > 0 ? PRIO_STATE_LAST : PRIO_STATE_FIRST;
		for (std::map<std::string, Module*>::iterator n = Modules.begin(); n != Modules.end(); ++n)
			n->second->Prioritize();

		if (prioritizationState == PRIO_STATE_LAST)
			break;
		if (tries == 19)
			ServerInstance->Logs->Log("MODULE", DEFAULT, "Hook priority dependency loop detected while loading " + newmod->ModuleSourceFile);
	}

	ServerInstance->BuildISupport();

	if (!state)
		return;

	for(std::vector<RestoreData>::iterator i = state->channelModes.begin(); i != state->channelModes.end(); i++)
	{
		Channel* c = ServerInstance->FindChan(i->item);
		ModeHandler* mh = ServerInstance->Modes->FindMode(i->name);
		if (c && mh)
		{
			irc::modestacker mc;
			mc.push(irc::modechange(mh->id, i->value, true));
			ServerInstance->SendMode(ServerInstance->FakeClient, c, mc, false);
		}
	}
	for(std::vector<RestoreData>::iterator i = state->channelExt.begin(); i != state->channelExt.end(); i++)
	{
		Channel* c = ServerInstance->FindChan(i->item);
		ExtensionItem* item = ServerInstance->Extensions.GetItem(i->name);
		if (c && item)
			item->unserialize(FORMAT_INTERNAL, c, i->value);
	}
	for(std::vector<RestoreData>::iterator i = state->userModes.begin(); i != state->userModes.end(); i++)
	{
		User* u = ServerInstance->FindUUID(i->item);
		ModeHandler* mh = ServerInstance->Modes->FindMode(i->name);
		if (u && mh)
		{
			irc::modestacker mc;
			mc.push(irc::modechange(mh->id, i->value, true));
			ServerInstance->SendMode(ServerInstance->FakeClient, u, mc, false);
		}
	}
	for(std::vector<RestoreData>::iterator i = state->userExt.begin(); i != state->userExt.end(); i++)
	{
		User* u = ServerInstance->FindUUID(i->item);
		ExtensionItem* item = ServerInstance->Extensions.GetItem(i->name);
		if (u && item)
			item->unserialize(FORMAT_INTERNAL, u, i->value);
	}
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
				DoSafeUnload(me->second, NULL);
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

void ConfigReadStatus::ReportError(const std::string& msg, bool Fatal)
{
	errors << msg << "\n";
	if (Fatal)
		fatal = true;
}

void ConfigReadStatus::ReportError(ConfigTag* where, const char* why, bool Fatal)
{
	if (where)
		ReportError("Error in <" + where->tag + "> tag at " + where->getTagLocation() + ": " + why, Fatal);
	else
		ReportError("Error in missing tag: " + std::string(why), Fatal);
}

ConfigTag* ConfigReadStatus::GetTag(const std::string& tag)
{
	ConfigTagList found = ServerInstance->Config->config_data.equal_range(tag);
	if (found.first == found.second)
		return NULL;
	ConfigTag* rv = found.first->second;
	found.first++;
	if (found.first != found.second)
		ReportError("Multiple <" + tag + "> tags found; only first will be used "
			"(first at " + rv->getTagLocation() + "; second at " + found.first->second->getTagLocation() + ")",
			false);
	return rv;
}

ConfigTagList ConfigReadStatus::GetTags(const std::string& key)
{
	return ServerInstance->Config->ConfTags(key);
}

void ModuleManager::AddService(ServiceProvider& item)
{
	Module* owner = item.creator;
	if (Modules.find(owner->ModuleSourceFile) == Modules.end())
		throw CoreException("Module is registering item in constructor; this must be done in init()!");

	switch (item.service)
	{
		case SERVICE_COMMAND:
			if (!ServerInstance->Parser->AddCommand(static_cast<Command*>(&item)))
				throw ModuleException("Command "+std::string(item.name)+" already exists.");
			return;
		case SERVICE_MODE:
			ServerInstance->Modes->AddMode(static_cast<ModeHandler*>(&item));
			return;
		case SERVICE_METADATA:
			ServerInstance->Extensions.Register(static_cast<ExtensionItem*>(&item));
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
		case SERVICE_MODE:
			return ServerInstance->Modes->FindMode(name);
		case SERVICE_COMMAND:
		{
			Commandtable::iterator i = ServerInstance->Parser->cmdlist.find(name);
			return i == ServerInstance->Parser->cmdlist.end() ? NULL : i->second;
		}
		case SERVICE_METADATA:
			return ServerInstance->Extensions.GetItem(name);
		default:
			throw ModuleException("Cannot find unknown service type");
	}
}

dynamic_reference_base::dynamic_reference_base(const std::string& Name)
	: name(Name), value(NULL)
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
	return value;
}

void PermissionData::SetReason(const char* format, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];

	va_start(argsPtr, format);
	vsnprintf(textbuffer, MAXBUF, format, argsPtr);
	va_end(argsPtr);

	reason = textbuffer;
}

void PermissionData::ErrorNumeric(int num, const char* format, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];

	int offset = snprintf(textbuffer, MAXBUF, ":%s %03d %s ",
		ServerInstance->Config->ServerName.c_str(), num, source->nick.c_str());

	va_start(argsPtr, format);
	vsnprintf(textbuffer + offset, MAXBUF - offset, format, argsPtr);
	va_end(argsPtr);

	reason = textbuffer;
}


void InspIRCd::SendMode(const std::vector<std::string>& parameters, User *src)
{
	Extensible* target;
	irc::modestacker modes;
	Modes->Parse(parameters, src, target, modes);
	Modes->Process(src, target, modes);
	Modes->Send(src,target, modes);
}

void InspIRCd::SendGlobalMode(const std::vector<std::string>& parameters, User *src)
{
	Extensible* target;
	irc::modestacker modes;
	Modes->Parse(parameters, src, target, modes);
	Modes->Process(src, target, modes);
	Modes->Send(src,target, modes);
	PI->SendMode(src, target, modes);
}

void InspIRCd::SendMode(User *src, Extensible* target, irc::modestacker& modes, bool global)
{
	Modes->Process(src, target, modes);
	Modes->Send(src,target, modes);
	if (global)
		PI->SendMode(src, target, modes);
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
	contentsize = 0;
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
	if ((x<0) || ((unsigned)x>fc.size()))
		return "";
	return fc[x];
}

int FileReader::FileSize()
{
	return fc.size();
}
