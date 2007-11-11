/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDmodules */

#include "inspircd.h"
#include "wildcard.h"
#include "xline.h"
#include "socket.h"
#include "socketengine.h"
#include "command_parse.h"
#include "dns.h"
#include "exitcodes.h"

#ifndef WIN32
	#include <dirent.h>
#endif

// version is a simple class for holding a modules version number
Version::Version(int major, int minor, int revision, int build, int flags, int api_ver)
: Major(major), Minor(minor), Revision(revision), Build(build), Flags(flags), API(api_ver)
{
}

Request::Request(char* anydata, Module* src, Module* dst)
: data(anydata), source(src), dest(dst)
{
	/* Ensure that because this module doesnt support ID strings, it doesnt break modules that do
	 * by passing them uninitialized pointers (could happen)
	 */
	id = '\0';
}

Request::Request(Module* src, Module* dst, const char* idstr)
: id(idstr), source(src), dest(dst)
{
}

char* Request::GetData()
{
	return this->data;
}

const char* Request::GetId()
{
	return this->id;
}

Module* Request::GetSource()
{
	return this->source;
}

Module* Request::GetDest()
{
	return this->dest;
}

char* Request::Send()
{
	if (this->dest)
	{
		return dest->OnRequest(this);
	}
	else
	{
		return NULL;
	}
}

Event::Event(char* anydata, Module* src, const std::string &eventid) : data(anydata), source(src), id(eventid) { }

char* Event::GetData()
{
	return (char*)this->data;
}

Module* Event::GetSource()
{
	return this->source;
}

char* Event::Send(InspIRCd* ServerInstance)
{
	FOREACH_MOD(I_OnEvent,OnEvent(this));
	return NULL;
}

std::string Event::GetEventID()
{
	return this->id;
}


// These declarations define the behavours of the base class Module (which does nothing at all)

		Module::Module(InspIRCd* Me) : ServerInstance(Me) { }
		Module::~Module() { }
void		Module::OnReadConfig(ServerConfig*, ConfigReader*) { }
int		Module::OnDownloadFile(const std::string&, std::istream*&) { return 0; }
void		Module::OnUserConnect(User*) { }
void		Module::OnUserQuit(User*, const std::string&, const std::string&) { }
void		Module::OnUserDisconnect(User*) { }
void		Module::OnUserJoin(User*, Channel*, bool, bool&) { }
void		Module::OnPostJoin(User*, Channel*) { }
void		Module::OnUserPart(User*, Channel*, const std::string&, bool&) { }
void		Module::OnRehash(User*, const std::string&) { }
void		Module::OnServerRaw(std::string&, bool, User*) { }
int		Module::OnUserPreJoin(User*, Channel*, const char*, std::string&) { return 0; }
void		Module::OnMode(User*, void*, int, const std::string&) { }
Version		Module::GetVersion() { return Version(1,0,0,0,VF_VENDOR,-1); }
void		Module::OnOper(User*, const std::string&) { }
void		Module::OnPostOper(User*, const std::string&) { }
void		Module::OnInfo(User*) { }
void		Module::OnWhois(User*, User*) { }
int		Module::OnUserPreInvite(User*, User*, Channel*) { return 0; }
int		Module::OnUserPreMessage(User*, void*, int, std::string&, char, CUList&) { return 0; }
int		Module::OnUserPreNotice(User*, void*, int, std::string&, char, CUList&) { return 0; }
int		Module::OnUserPreNick(User*, const std::string&) { return 0; }
void		Module::OnUserPostNick(User*, const std::string&) { }
int		Module::OnAccessCheck(User*, User*, Channel*, int) { return ACR_DEFAULT; }
void		Module::On005Numeric(std::string&) { }
int		Module::OnKill(User*, User*, const std::string&) { return 0; }
void		Module::OnLoadModule(Module*, const std::string&) { }
void		Module::OnUnloadModule(Module*, const std::string&) { }
void		Module::OnBackgroundTimer(time_t) { }
int		Module::OnPreCommand(const std::string&, const char**, int, User *, bool, const std::string&) { return 0; }
void		Module::OnPostCommand(const std::string&, const char**, int, User *, CmdResult, const std::string&) { }
bool		Module::OnCheckReady(User*) { return true; }
int		Module::OnUserRegister(User*) { return 0; }
int		Module::OnUserPreKick(User*, User*, Channel*, const std::string&) { return 0; }
void		Module::OnUserKick(User*, User*, Channel*, const std::string&, bool&) { }
int		Module::OnCheckInvite(User*, Channel*) { return 0; }
int		Module::OnCheckKey(User*, Channel*, const std::string&) { return 0; }
int		Module::OnCheckLimit(User*, Channel*) { return 0; }
int		Module::OnCheckBan(User*, Channel*) { return 0; }
int		Module::OnStats(char, User*, string_list&) { return 0; }
int		Module::OnChangeLocalUserHost(User*, const std::string&) { return 0; }
int		Module::OnChangeLocalUserGECOS(User*, const std::string&) { return 0; }
int		Module::OnLocalTopicChange(User*, Channel*, const std::string&) { return 0; }
void		Module::OnEvent(Event*) { return; }
char*		Module::OnRequest(Request*) { return NULL; }
int		Module::OnOperCompare(const std::string&, const std::string&, int) { return 0; }
void		Module::OnGlobalOper(User*) { }
void		Module::OnPostConnect(User*) { }
int		Module::OnAddBan(User*, Channel*, const std::string &) { return 0; }
int		Module::OnDelBan(User*, Channel*, const std::string &) { return 0; }
void		Module::OnRawSocketAccept(int, const std::string&, int) { }
int		Module::OnRawSocketWrite(int, const char*, int) { return 0; }
void		Module::OnRawSocketClose(int) { }
void		Module::OnRawSocketConnect(int) { }
int		Module::OnRawSocketRead(int, char*, unsigned int, int&) { return 0; }
void		Module::OnUserMessage(User*, void*, int, const std::string&, char, const CUList&) { }
void		Module::OnUserNotice(User*, void*, int, const std::string&, char, const CUList&) { }
void 		Module::OnRemoteKill(User*, User*, const std::string&, const std::string&) { }
void		Module::OnUserInvite(User*, User*, Channel*) { }
void		Module::OnPostLocalTopicChange(User*, Channel*, const std::string&) { }
void		Module::OnGetServerDescription(const std::string&, std::string&) { }
void		Module::OnSyncUser(User*, Module*, void*) { }
void		Module::OnSyncChannel(Channel*, Module*, void*) { }
void		Module::ProtoSendMode(void*, int, void*, const std::string&) { }
void		Module::OnSyncChannelMetaData(Channel*, Module*, void*, const std::string&, bool) { }
void		Module::OnSyncUserMetaData(User*, Module*, void*, const std::string&, bool) { }
void		Module::OnSyncOtherMetaData(Module*, void*, bool) { }
void		Module::OnDecodeMetaData(int, void*, const std::string&, const std::string&) { }
void		Module::ProtoSendMetaData(void*, int, void*, const std::string&, const std::string&) { }
void		Module::OnWallops(User*, const std::string&) { }
void		Module::OnChangeHost(User*, const std::string&) { }
void		Module::OnChangeName(User*, const std::string&) { }
void		Module::OnAddLine(User*, XLine*) { }
void		Module::OnDelLine(User*, XLine*) { }
void 		Module::OnCleanup(int, void*) { }
void		Module::OnChannelDelete(Channel*) { }
void		Module::OnSetAway(User*) { }
void		Module::OnCancelAway(User*) { }
int		Module::OnUserList(User*, Channel*, CUList*&) { return 0; }
int		Module::OnWhoisLine(User*, User*, int&, std::string&) { return 0; }
void		Module::OnBuildExemptList(MessageType, Channel*, User*, char, CUList&, const std::string&) { }
void		Module::OnGarbageCollect() { }
void		Module::OnBufferFlushed(User*) { }
void 		Module::OnText(User*, void*, int, const std::string&, char, CUList&) { }


ModuleManager::ModuleManager(InspIRCd* Ins) : ModCount(0), Instance(Ins)
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

bool ModuleManager::SetPriority(Module* mod, PriorityState s)
{
	for (size_t n = I_BEGIN + 1; n != I_END; ++n)
		SetPriority(mod, (Implementation)n, s);

	return true;
}

bool ModuleManager::SetPriority(Module* mod, Implementation i, PriorityState s, Module** modules, size_t sz)
{
	/** To change the priority of a module, we first find its position in the vector,
	 * then we find the position of the other modules in the vector that this module
	 * wants to be before/after. We pick off either the first or last of these depending
	 * on which they want, and we make sure our module is *at least* before or after
	 * the first or last of this subset, depending again on the type of priority.
	 */
	size_t swap_pos;
	size_t source = 0;
	bool swap = true;
	bool found = false;

	/* Locate our module. This is O(n) but it only occurs on module load so we're
	 * not too bothered about it
	 */
	for (size_t x = 0; x != EventHandlers[i].size(); ++x)
	{
		if (EventHandlers[i][x] == mod)
		{
			source = x;
			found = true;
			break;
		}
	}

	/* Eh? this module doesnt exist, probably trying to set priority on an event
	 * theyre not attached to.
	 */
	if (!found)
		return false;

	switch (s)
	{
		/* Dummy value */
		case PRIO_DONTCARE:
			swap = false;
		break;
		/* Module wants to be first, sod everything else */
		case PRIO_FIRST:
			swap_pos = 0;
		break;
		/* Module is submissive and wants to be last... awww. */
		case PRIO_LAST:
			if (EventHandlers[i].empty())
				swap_pos = 0;
			else
				swap_pos = EventHandlers[i].size() - 1;
		break;
		/* Place this module after a set of other modules */
		case PRIO_AFTER:
		{
			/* Find the latest possible position */
			swap_pos = 0;
			swap = false;
			for (size_t x = 0; x != EventHandlers[i].size(); ++x)
			{
				for (size_t n = 0; n < sz; ++n)
				{
					if ((modules[n]) && (EventHandlers[i][x] == modules[n]) && (x >= swap_pos) && (source <= swap_pos))
					{
						swap_pos = x;
						swap = true;
					}
				}
			}
		}
		break;
		/* Place this module before a set of other modules */
		case PRIO_BEFORE:
		{
			swap_pos = EventHandlers[i].size() - 1;
			swap = false;
			for (size_t x = 0; x != EventHandlers[i].size(); ++x)
			{
				for (size_t n = 0; n < sz; ++n)
				{
					if ((modules[n]) && (EventHandlers[i][x] == modules[n]) && (x <= swap_pos) && (source >= swap_pos))
					{
						swap = true;
						swap_pos = x;
					}
				}
			}
		}
		break;
	}

	/* Do we need to swap? */
	if (swap && (swap_pos != source))
		std::swap(EventHandlers[i][swap_pos], EventHandlers[i][source]);

	return true;
}

std::string& ModuleManager::LastError()
{
	return LastModuleError;
}

bool ModuleManager::Load(const char* filename)
{
	/* Do we have a glob pattern in the filename?
	 * The user wants to load multiple modules which
	 * match the pattern.
	 */
	if (strchr(filename,'*') || (strchr(filename,'?')))
	{
		int n_match = 0;
		DIR* library = opendir(Instance->Config->ModPath);
		if (library)
		{
			/* Try and locate and load all modules matching the pattern */
			dirent* entry = NULL;
			while ((entry = readdir(library)))
			{
				if (Instance->MatchText(entry->d_name, filename))
				{
					if (!this->Load(entry->d_name))
						n_match++;
				}
			}
			closedir(library);
		}
		/* Loadmodule will now return false if any one of the modules failed
		 * to load (but wont abort when it encounters a bad one) and when 1 or
		 * more modules were actually loaded.
		 */
		return (n_match > 0);
	}

	char modfile[MAXBUF];
	snprintf(modfile,MAXBUF,"%s/%s",Instance->Config->ModPath,filename);
	std::string filename_str = filename;

	if (!ServerConfig::DirValid(modfile))
	{
		LastModuleError = "Module " + filename_str + " is not within the modules directory.";
		Instance->Log(DEFAULT, LastModuleError);
		return false;
	}
	
	if (!ServerConfig::FileExists(modfile))
	{
		LastModuleError = "Module file could not be found: " + filename_str;
		Instance->Log(DEFAULT, LastModuleError);
		return false;
	}
	
	if (Modules.find(filename_str) != Modules.end())
	{	
		LastModuleError = "Module " + filename_str + " is already loaded, cannot load a module twice!";
		Instance->Log(DEFAULT, LastModuleError);
		return false;
	}
		
	Module* newmod = NULL;
	ircd_module* newhandle = NULL;

	try
	{
		/* This will throw a CoreException if there's a problem loading
		 * the module file or getting a pointer to the init_module symbol.
		 */
		newhandle = new ircd_module(Instance, modfile, "init_module");
		newmod = newhandle->CallInit();

		if (newmod)
		{
			Version v = newmod->GetVersion();

			if (v.API != API_VERSION)
			{
				DetachAll(newmod);
				delete newmod;
				delete newhandle;
				LastModuleError = "Unable to load " + filename_str + ": Incorrect module API version: " + ConvToStr(v.API) + " (our version: " + ConvToStr(API_VERSION) + ")";
				Instance->Log(DEFAULT, LastModuleError);
				return false;
			}
			else
			{
				Instance->Log(DEFAULT,"New module introduced: %s (API version %d, Module version %d.%d.%d.%d)%s", filename, v.API, v.Major, v.Minor, v.Revision, v.Build, (!(v.Flags & VF_VENDOR) ? " [3rd Party]" : " [Vendor]"));
			}

			Modules[filename_str] = std::make_pair(newhandle, newmod);
		}
		else
		{
			delete newhandle;
			LastModuleError = "Unable to load " + filename_str + ": Probably missing init_module() entrypoint, but dlsym() didn't notice a problem";
			Instance->Log(DEFAULT, LastModuleError);
			return false;
		}
	}
	/** XXX: Is there anything we can do about this mess? -- Brain */
	catch (LoadModuleException& modexcept)
	{
		DetachAll(newmod);
		if (newmod)
			delete newmod;
		if (newhandle)
			delete newhandle;
		LastModuleError = "Unable to load " + filename_str + ": Error when loading: " + modexcept.GetReason();
		Instance->Log(DEFAULT, LastModuleError);
		return false;
	}
	catch (FindSymbolException& modexcept)
	{
		DetachAll(newmod);
		if (newmod)
			delete newmod;
		if (newhandle)
			delete newhandle;
		LastModuleError = "Unable to load " + filename_str + ": Error finding symbol: " + modexcept.GetReason();
		Instance->Log(DEFAULT, LastModuleError);
		return false;
	}
	catch (CoreException& modexcept)
	{
		DetachAll(newmod);
		if (newmod)
			delete newmod;
		if (newhandle)
			delete newhandle;
		LastModuleError = "Unable to load " + filename_str + ": " + modexcept.GetReason();
		Instance->Log(DEFAULT, LastModuleError);
		return false;
	}

	this->ModCount++;
	FOREACH_MOD_I(Instance,I_OnLoadModule,OnLoadModule(newmod, filename_str));

	/* We give every module a chance to re-prioritize when we introduce a new one,
	 * not just the one thats loading, as the new module could affect the preference
	 * of others
	 */
	for (std::map<std::string, std::pair<ircd_module*, Module*> >::iterator n = Modules.begin(); n != Modules.end(); ++n)
		n->second.second->Prioritize();

	Instance->BuildISupport();
	return true;
}

bool ModuleManager::Unload(const char* filename)
{
	std::string filename_str(filename);
	std::map<std::string, std::pair<ircd_module*, Module*> >::iterator modfind = Modules.find(filename);

	if (modfind != Modules.end())
	{
		if (modfind->second.second->GetVersion().Flags & VF_STATIC)
		{
			LastModuleError = "Module " + filename_str + " not unloadable (marked static)";
			Instance->Log(DEFAULT, LastModuleError);
			return false;
		}
		std::pair<int,std::string> intercount = GetInterfaceInstanceCount(modfind->second.second);
		if (intercount.first > 0)
		{
			LastModuleError = "Failed to unload module " + filename_str + ", being used by " + ConvToStr(intercount.first) + " other(s) via interface '" + intercount.second + "'";
			Instance->Log(DEFAULT, LastModuleError);
			return false;
		}

		/* Give the module a chance to tidy out all its metadata */
		for (chan_hash::iterator c = Instance->chanlist->begin(); c != Instance->chanlist->end(); c++)
		{
			modfind->second.second->OnCleanup(TYPE_CHANNEL,c->second);
		}
		for (user_hash::iterator u = Instance->clientlist->begin(); u != Instance->clientlist->end(); u++)
		{
			modfind->second.second->OnCleanup(TYPE_USER,u->second);
		}

		/* Tidy up any dangling resolvers */
		Instance->Res->CleanResolvers(modfind->second.second);


		FOREACH_MOD_I(Instance,I_OnUnloadModule,OnUnloadModule(modfind->second.second, modfind->first));

		this->DetachAll(modfind->second.second);

		Instance->Parser->RemoveCommands(filename);

		delete modfind->second.second;
		delete modfind->second.first;
		Modules.erase(modfind);

		Instance->Log(DEFAULT,"Module %s unloaded",filename);
		this->ModCount--;
		Instance->BuildISupport();
		return true;
	}

	LastModuleError = "Module " + filename_str + " is not loaded, cannot unload it!";
	Instance->Log(DEFAULT, LastModuleError);
	return false;
}

/* We must load the modules AFTER initializing the socket engine, now */
void ModuleManager::LoadAll()
{
	char configToken[MAXBUF];
	ModCount = -1;

	for(int count = 0; count < Instance->Config->ConfValueEnum(Instance->Config->config_data, "module"); count++)
	{
		Instance->Config->ConfValue(Instance->Config->config_data, "module", "name", count, configToken, MAXBUF);
		printf_c("[\033[1;32m*\033[0m] Loading module:\t\033[1;32m%s\033[0m\n",configToken);
		
		if (!this->Load(configToken))		
		{
			Instance->Log(DEFAULT, this->LastError());
			printf_c("\n[\033[1;31m*\033[0m] %s\n\n", this->LastError().c_str());
			Instance->Exit(EXIT_STATUS_MODULE);
		}
	}
}

bool ModuleManager::PublishFeature(const std::string &FeatureName, Module* Mod)
{
	if (Features.find(FeatureName) == Features.end())
	{
		Features[FeatureName] = Mod;
		return true;
	}
	return false;
}

bool ModuleManager::UnpublishFeature(const std::string &FeatureName)
{
	featurelist::iterator iter = Features.find(FeatureName);
	
	if (iter == Features.end())
		return false;

	Features.erase(iter);
	return true;
}

Module* ModuleManager::FindFeature(const std::string &FeatureName)
{
	featurelist::iterator iter = Features.find(FeatureName);

	if (iter == Features.end())
		return NULL;

	return iter->second;
}

bool ModuleManager::PublishInterface(const std::string &InterfaceName, Module* Mod)
{
	interfacelist::iterator iter = Interfaces.find(InterfaceName);

	if (iter == Interfaces.end())
	{
		modulelist ml;
		ml.push_back(Mod);
		Interfaces[InterfaceName] = std::make_pair(0, ml);
		return true;
	}
	else
	{
		iter->second.second.push_back(Mod);
		return true;
	}
	return false;
}

bool ModuleManager::UnpublishInterface(const std::string &InterfaceName, Module* Mod)
{
	interfacelist::iterator iter = Interfaces.find(InterfaceName);

	if (iter == Interfaces.end())
		return false;

	for (modulelist::iterator x = iter->second.second.begin(); x != iter->second.second.end(); x++)
	{
		if (*x == Mod)
		{
			iter->second.second.erase(x);
			if (iter->second.second.empty())
				Interfaces.erase(InterfaceName);
			return true;
		}
	}
	return false;
}

modulelist* ModuleManager::FindInterface(const std::string &InterfaceName)
{
	interfacelist::iterator iter = Interfaces.find(InterfaceName);
	if (iter == Interfaces.end())
		return NULL;
	else
		return &(iter->second.second);
}

void ModuleManager::UseInterface(const std::string &InterfaceName)
{
	interfacelist::iterator iter = Interfaces.find(InterfaceName);
	if (iter != Interfaces.end())
		iter->second.first++;

}

void ModuleManager::DoneWithInterface(const std::string &InterfaceName)
{
	interfacelist::iterator iter = Interfaces.find(InterfaceName);
	if (iter != Interfaces.end())
		iter->second.first--;
}

std::pair<int,std::string> ModuleManager::GetInterfaceInstanceCount(Module* m)
{
	for (interfacelist::iterator iter = Interfaces.begin(); iter != Interfaces.end(); iter++)
	{
		for (modulelist::iterator x = iter->second.second.begin(); x != iter->second.second.end(); x++)
		{
			if (*x == m)
			{
				return std::make_pair(iter->second.first, iter->first);
			}
		}
	}
	return std::make_pair(0, "");
}

const std::string& ModuleManager::GetModuleName(Module* m)
{
	static std::string nothing;

	for (std::map<std::string, std::pair<ircd_module*, Module*> >::iterator n = Modules.begin(); n != Modules.end(); ++n)
	{
		if (n->second.second == m)
			return n->first;
	}

	return nothing;
}

/* This is ugly, yes, but hash_map's arent designed to be
 * addressed in this manner, and this is a bit of a kludge.
 * Luckily its a specialist function and rarely used by
 * many modules (in fact, it was specially created to make
 * m_safelist possible, initially).
 */

Channel* InspIRCd::GetChannelIndex(long index)
{
	int target = 0;
	for (chan_hash::iterator n = this->chanlist->begin(); n != this->chanlist->end(); n++, target++)
	{
		if (index == target)
			return n->second;
	}
	return NULL;
}

bool InspIRCd::MatchText(const std::string &sliteral, const std::string &spattern)
{
	return match(sliteral.c_str(),spattern.c_str());
}

CmdResult InspIRCd::CallCommandHandler(const std::string &commandname, const char** parameters, int pcnt, User* user)
{
	return this->Parser->CallHandler(commandname,parameters,pcnt,user);
}

bool InspIRCd::IsValidModuleCommand(const std::string &commandname, int pcnt, User* user)
{
	return this->Parser->IsValidCommand(commandname, pcnt, user);
}

void InspIRCd::AddCommand(Command *f)
{
	if (!this->Parser->CreateCommand(f))
	{
		ModuleException err("Command "+std::string(f->command)+" already exists.");
		throw (err);
	}
}

void InspIRCd::SendMode(const char** parameters, int pcnt, User *user)
{
	this->Modes->Process(parameters,pcnt,user,true);
}

void InspIRCd::DumpText(User* User, const std::string &LinePrefix, stringstream &TextStream)
{
	std::string CompleteLine = LinePrefix;
	std::string Word;
	while (TextStream >> Word)
	{
		if (CompleteLine.length() + Word.length() + 3 > 500)
		{
			User->WriteServ(CompleteLine);
			CompleteLine = LinePrefix;
		}
		CompleteLine = CompleteLine + Word + " ";
	}
	User->WriteServ(CompleteLine);
}

User* FindDescriptorHandler::Call(int socket)
{
	return reinterpret_cast<User*>(Server->SE->GetRef(socket));
}

bool InspIRCd::AddMode(ModeHandler* mh)
{
	return this->Modes->AddMode(mh);
}

bool InspIRCd::AddModeWatcher(ModeWatcher* mw)
{
	return this->Modes->AddModeWatcher(mw);
}

bool InspIRCd::DelModeWatcher(ModeWatcher* mw)
{
	return this->Modes->DelModeWatcher(mw);
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
	std::map<std::string, std::pair<ircd_module*, Module*> >::iterator modfind = Modules.find(name);

	if (modfind == Modules.end())
		return NULL;
	else
		return modfind->second.second;
}

const std::vector<std::string> ModuleManager::GetAllModuleNames(int filter)
{
	std::vector<std::string> retval;
	for (std::map<std::string, std::pair<ircd_module*, Module*> >::iterator x = Modules.begin(); x != Modules.end(); ++x)
		if (!filter || (x->second.second->GetVersion().Flags & filter))
			retval.push_back(x->first);
	return retval;
}

ConfigReader::ConfigReader(InspIRCd* Instance) : ServerInstance(Instance)
{
	/* Is there any reason to load the entire config file again here?
	 * it's needed if they specify another config file, but using the
	 * default one we can just use the global config data - pre-parsed!
	 */
	this->errorlog = new std::ostringstream(std::stringstream::in | std::stringstream::out);
	this->error = CONF_NO_ERROR;
	this->data = &ServerInstance->Config->config_data;
	this->privatehash = false;
}


ConfigReader::~ConfigReader()
{
	if (this->errorlog)
		delete this->errorlog;
	if(this->privatehash)
		delete this->data;
}


ConfigReader::ConfigReader(InspIRCd* Instance, const std::string &filename) : ServerInstance(Instance)
{
	ServerInstance->Config->ClearStack();

	this->error = CONF_NO_ERROR;
	this->data = new ConfigDataHash;
	this->privatehash = true;
	this->errorlog = new std::ostringstream(std::stringstream::in | std::stringstream::out);
	for (int pass = 0; pass < 2; pass++)
	{
		/*** XXX: Can return a 'not ready yet!' code! */
		this->readerror = ServerInstance->Config->LoadConf(*this->data, filename, *this->errorlog, pass);
	}
	if (!this->readerror)
		this->error = CONF_FILE_NOT_FOUND;
}


std::string ConfigReader::ReadValue(const std::string &tag, const std::string &name, const std::string &default_value, int index, bool allow_linefeeds)
{
	/* Don't need to strlcpy() tag and name anymore, ReadConf() takes const char* */ 
	std::string result;
	
	if (!ServerInstance->Config->ConfValue(*this->data, tag, name, default_value, index, result, allow_linefeeds))
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
	return ServerInstance->Config->ConfValueBool(*this->data, tag, name, default_value, index);
}

bool ConfigReader::ReadFlag(const std::string &tag, const std::string &name, int index)
{
	return ReadFlag(tag, name, "", index);
}


int ConfigReader::ReadInteger(const std::string &tag, const std::string &name, const std::string &default_value, int index, bool need_positive)
{
	int result;
	
	if(!ServerInstance->Config->ConfValueInteger(*this->data, tag, name, default_value, index, result))
	{
		this->error = CONF_VALUE_NOT_FOUND;
		return 0;
	}
	
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

void ConfigReader::DumpErrors(bool bail, User* user)
{
	ServerInstance->Config->ReportConfigError(this->errorlog->str(), bail, user);
}


int ConfigReader::Enumerate(const std::string &tag)
{
	return ServerInstance->Config->ConfValueEnum(*this->data, tag);
}

int ConfigReader::EnumerateValues(const std::string &tag, int index)
{
	return ServerInstance->Config->ConfVarEnum(*this->data, tag, index);
}

bool ConfigReader::Verify()
{
	return this->readerror;
}


FileReader::FileReader(InspIRCd* Instance, const std::string &filename) : ServerInstance(Instance)
{
	LoadFile(filename);
}

FileReader::FileReader(InspIRCd* Instance) : ServerInstance(Instance)
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
	file_cache c;
	c.clear();
	if (ServerInstance->Config->ReadFile(c,filename.c_str()))
	{
		this->fc = c;
		this->CalcSize();
	}
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
