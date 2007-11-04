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
void		Module::OnUserConnect(User*) { }
void		Module::OnUserQuit(User*, const std::string&, const std::string&) { }
void		Module::OnUserDisconnect(User*) { }
void		Module::OnUserJoin(User*, Channel*, bool&) { }
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
void		Module::Implements(char* Implements) { for (int j = 0; j < 255; j++) Implements[j] = 0; }
void		Module::OnChannelDelete(Channel*) { }
Priority	Module::Prioritize() { return PRIORITY_DONTCARE; }
void		Module::OnSetAway(User*) { }
void		Module::OnCancelAway(User*) { }
int		Module::OnUserList(User*, Channel*, CUList*&) { return 0; }
int		Module::OnWhoisLine(User*, User*, int&, std::string&) { return 0; }
void		Module::OnBuildExemptList(MessageType, Channel*, User*, char, CUList&, const std::string&) { }
void		Module::OnGarbageCollect() { }
void		Module::OnBufferFlushed(User*) { }
void 		Module::OnText(User*, void*, int, const std::string&, char, CUList&) { }


ModuleManager::ModuleManager(InspIRCd* Ins)
: ModCount(0), Instance(Ins)
{
	for (int n = I_BEGIN + 1; n != I_END; ++n)
		EventHandlers.push_back(std::list<Module*>());
}

ModuleManager::~ModuleManager()
{
}

const char* ModuleManager::LastError()
{
	return MODERR;
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
		snprintf(MODERR, MAXBUF,"Module %s is not within the modules directory.", modfile);
		Instance->Log(DEFAULT, MODERR);
		return false;
	}
	
	if (!ServerConfig::FileExists(modfile))
	{
		snprintf(MODERR,MAXBUF,"Module file could not be found: %s", modfile);
		Instance->Log(DEFAULT, MODERR);
		return false;
	}
	
	if(find(Instance->Config->module_names.begin(), Instance->Config->module_names.end(), filename_str) != Instance->Config->module_names.end())
	{	
		Instance->Log(DEFAULT,"Module %s is already loaded, cannot load a module twice!",modfile);
		snprintf(MODERR, MAXBUF, "Module already loaded");
		return false;
	}
		
	Module* newmod;
	ircd_module* newhandle;
	
	newmod = NULL;
	newhandle = NULL;
		
	try
	{
		/* This will throw a CoreException if there's a problem loading
		 * the module file or getting a pointer to the init_module symbol.
		 */
		newhandle = new ircd_module(Instance, modfile, "init_module");
			
		handles[this->ModCount+1] = newhandle;
			
		newmod = handles[this->ModCount+1]->CallInit();

		if(newmod)
		{
			Version v = newmod->GetVersion();

			if (v.API != API_VERSION)
			{
				delete newmod;
				Instance->Log(DEFAULT,"Unable to load %s: Incorrect module API version: %d (our version: %d)",modfile,v.API,API_VERSION);
				snprintf(MODERR,MAXBUF,"Loader/Linker error: Incorrect module API version: %d (our version: %d)",v.API,API_VERSION);
				return false;
			}
			else
			{
				Instance->Log(DEFAULT,"New module introduced: %s (API version %d, Module version %d.%d.%d.%d)%s", filename, v.API, v.Major, v.Minor, v.Revision, v.Build, (!(v.Flags & VF_VENDOR) ? " [3rd Party]" : " [Vendor]"));
			}

			modules[this->ModCount+1] = newmod;
				
			/* save the module and the module's classfactory, if
			 * this isnt done, random crashes can occur :/ */
			Instance->Config->module_names.push_back(filename);

			char* x = &Instance->Config->implement_lists[this->ModCount+1][0];
			for(int t = 0; t < 255; t++)
				x[t] = 0;

			modules[this->ModCount+1]->Implements(x);

			for(int t = 0; t < 255; t++)
				Instance->Config->global_implementation[t] += Instance->Config->implement_lists[this->ModCount+1][t];
		}
		else
		{
			Instance->Log(DEFAULT, "Unable to load %s",modfile);
			snprintf(MODERR,MAXBUF, "Probably missing init_module() entrypoint, but dlsym() didn't notice a problem");
			return false;
		}
	}
	catch (LoadModuleException& modexcept)
	{
		Instance->Log(DEFAULT,"Unable to load %s: %s", modfile, modexcept.GetReason());
		snprintf(MODERR,MAXBUF,"Loader/Linker error: %s", modexcept.GetReason());
		return false;
	}
	catch (FindSymbolException& modexcept)
	{
		Instance->Log(DEFAULT,"Unable to load %s: %s", modfile, modexcept.GetReason());
		snprintf(MODERR,MAXBUF,"Loader/Linker error: %s", modexcept.GetReason());
		return false;
	}
	catch (CoreException& modexcept)
	{
		Instance->Log(DEFAULT,"Unable to load %s: %s",modfile,modexcept.GetReason());
		snprintf(MODERR,MAXBUF,"Factory function of %s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
		return false;
	}
	
	this->ModCount++;
	FOREACH_MOD_I(Instance,I_OnLoadModule,OnLoadModule(modules[this->ModCount],filename_str));
	// now work out which modules, if any, want to move to the back of the queue,
	// and if they do, move them there.
	std::vector<std::string> put_to_back;
	std::vector<std::string> put_to_front;
	std::map<std::string,std::string> put_before;
	std::map<std::string,std::string> put_after;
	for (unsigned int j = 0; j < Instance->Config->module_names.size(); j++)
	{
		if (modules[j]->Prioritize() == PRIORITY_LAST)
			put_to_back.push_back(Instance->Config->module_names[j]);
		else if (modules[j]->Prioritize() == PRIORITY_FIRST)
			put_to_front.push_back(Instance->Config->module_names[j]);
		else if ((modules[j]->Prioritize() & 0xFF) == PRIORITY_BEFORE)
			put_before[Instance->Config->module_names[j]] = Instance->Config->module_names[modules[j]->Prioritize() >> 8];
		else if ((modules[j]->Prioritize() & 0xFF) == PRIORITY_AFTER)
			put_after[Instance->Config->module_names[j]] = Instance->Config->module_names[modules[j]->Prioritize() >> 8];
	}
	for (unsigned int j = 0; j < put_to_back.size(); j++)
		MoveToLast(put_to_back[j]);
	for (unsigned int j = 0; j < put_to_front.size(); j++)
		MoveToFirst(put_to_front[j]);
	for (std::map<std::string,std::string>::iterator j = put_before.begin(); j != put_before.end(); j++)
		MoveBefore(j->first,j->second);
	for (std::map<std::string,std::string>::iterator j = put_after.begin(); j != put_after.end(); j++)
		MoveAfter(j->first,j->second);
	Instance->BuildISupport();
	return true;
}

bool ModuleManager::EraseHandle(unsigned int j)
{
	ModuleHandleList::iterator iter;
	
	if (j >= handles.size())
	{
		return false;
	}
	
	iter = handles.begin() + j;

	if(*iter)
	{
		delete *iter;	
		handles.erase(iter);
 		handles.push_back(NULL);
	}

	return true;
}

bool ModuleManager::EraseModule(unsigned int j)
{
	bool success = false;
	
	ModuleList::iterator iter;	
	
	if (j >= modules.size())
	{
		return false;
	}

	iter = modules.begin() + j;

	if (*iter)
	{
		delete *iter;	
		modules.erase(iter);
 		modules.push_back(NULL);
		success = true;
	}

	std::vector<std::string>::iterator iter2;
	
	if (j >= Instance->Config->module_names.size())
	{
		return false;
	}

	iter2 = Instance->Config->module_names.begin() + j;

	Instance->Config->module_names.erase(iter2);
	success = true;

	return success;
}

void ModuleManager::MoveTo(std::string modulename,int slot)
{
	unsigned int v2 = 256;
	
	for (unsigned int v = 0; v < Instance->Config->module_names.size(); v++)
	{
		if (Instance->Config->module_names[v] == modulename)
		{
			// found an instance, swap it with the item at the end
			v2 = v;
			break;
		}
	}
	if ((v2 != (unsigned int)slot) && (v2 < 256))
	{
		// Swap the module names over
		Instance->Config->module_names[v2] = Instance->Config->module_names[slot];
		Instance->Config->module_names[slot] = modulename;
		// now swap the module factories
		ircd_module* temp = handles[v2];
		handles[v2] = handles[slot];
		handles[slot] = temp;
		// now swap the module objects
		Module* temp_module = modules[v2];
		modules[v2] = modules[slot];
		modules[slot] = temp_module;
		// now swap the implement lists (we dont
		// need to swap the global or recount it)
		for (int n = 0; n < 255; n++)
		{
			char x = Instance->Config->implement_lists[v2][n];
			Instance->Config->implement_lists[v2][n] = Instance->Config->implement_lists[slot][n];
			Instance->Config->implement_lists[slot][n] = x;
		}
	}
}

void ModuleManager::MoveAfter(std::string modulename, std::string after)
{
	for (unsigned int v = 0; v < Instance->Config->module_names.size(); v++)
	{
		if (Instance->Config->module_names[v] == after)
		{
			MoveTo(modulename, v);
			return;
		}
	}
}

void ModuleManager::MoveBefore(std::string modulename, std::string before)
{
	for (unsigned int v = 0; v < Instance->Config->module_names.size(); v++)
	{
		if (Instance->Config->module_names[v] == before)
		{
			if (v > 0)
			{
				MoveTo(modulename, v-1);
			}
			else
			{
				MoveTo(modulename, v);
			}
			return;
		}
	}
}

void ModuleManager::MoveToFirst(std::string modulename)
{
	MoveTo(modulename,0);
}

void ModuleManager::MoveToLast(std::string modulename)
{
	MoveTo(modulename,this->GetCount());
}

bool ModuleManager::Unload(const char* filename)
{
	std::string filename_str = filename;
	for (unsigned int j = 0; j != Instance->Config->module_names.size(); j++)
	{
		if (Instance->Config->module_names[j] == filename_str)
		{
			if (modules[j]->GetVersion().Flags & VF_STATIC)
			{
				Instance->Log(DEFAULT,"Failed to unload STATIC module %s",filename);
				snprintf(MODERR,MAXBUF,"Module not unloadable (marked static)");
				return false;
			}
			std::pair<int,std::string> intercount = GetInterfaceInstanceCount(modules[j]);
			if (intercount.first > 0)
			{
				Instance->Log(DEFAULT,"Failed to unload module %s, being used by %d other(s) via interface '%s'",filename, intercount.first, intercount.second.c_str());
				snprintf(MODERR,MAXBUF,"Module not unloadable (Still in use by %d other module%s which %s using its interface '%s') -- unload dependent modules first!",
						intercount.first,
						intercount.first > 1 ? "s" : "",
						intercount.first > 1 ? "are" : "is",
						intercount.second.c_str());
				return false;
			}
			/* Give the module a chance to tidy out all its metadata */
			for (chan_hash::iterator c = Instance->chanlist->begin(); c != Instance->chanlist->end(); c++)
			{
				modules[j]->OnCleanup(TYPE_CHANNEL,c->second);
			}
			for (user_hash::iterator u = Instance->clientlist->begin(); u != Instance->clientlist->end(); u++)
			{
				modules[j]->OnCleanup(TYPE_USER,u->second);
			}

			/* Tidy up any dangling resolvers */
			Instance->Res->CleanResolvers(modules[j]);

			FOREACH_MOD_I(Instance,I_OnUnloadModule,OnUnloadModule(modules[j],Instance->Config->module_names[j]));

			for(int t = 0; t < 255; t++)
			{
				Instance->Config->global_implementation[t] -= Instance->Config->implement_lists[j][t];
			}

			/* We have to renumber implement_lists after unload because the module numbers change!
			 */
			for(int j2 = j; j2 < 254; j2++)
			{
				for(int t = 0; t < 255; t++)
				{
					Instance->Config->implement_lists[j2][t] = Instance->Config->implement_lists[j2+1][t];
				}
			}

			// found the module
			Instance->Parser->RemoveCommands(filename);
			this->EraseModule(j);
			this->EraseHandle(j);
			Instance->Log(DEFAULT,"Module %s unloaded",filename);
			this->ModCount--;
			Instance->BuildISupport();
			return true;
		}
	}
	Instance->Log(DEFAULT,"Module %s is not loaded, cannot unload it!",filename);
	snprintf(MODERR,MAXBUF,"Module not loaded");
	return false;
}

/* We must load the modules AFTER initializing the socket engine, now */
void ModuleManager::LoadAll()
{
	char configToken[MAXBUF];
	Instance->Config->module_names.clear();
	ModCount = -1;

	for(int count = 0; count < Instance->Config->ConfValueEnum(Instance->Config->config_data, "module"); count++)
	{
		Instance->Config->ConfValue(Instance->Config->config_data, "module", "name", count, configToken, MAXBUF);
		printf_c("[\033[1;32m*\033[0m] Loading module:\t\033[1;32m%s\033[0m\n",configToken);
		
		if (!this->Load(configToken))		
		{
			Instance->Log(DEFAULT,"There was an error loading the module '%s': %s", configToken, this->LastError());
			printf_c("\n[\033[1;31m*\033[0m] There was an error loading the module '%s': %s\n\n", configToken, this->LastError());
			Instance->Exit(EXIT_STATUS_MODULE);
		}
	}
	printf_c("\nA total of \033[1;32m%d\033[0m module%s been loaded.\n", (this->GetCount()+1), (this->GetCount()+1) == 1 ? " has" : "s have");
	Instance->Log(DEFAULT,"Total loaded modules: %d", this->GetCount()+1);
}

long ModuleManager::PriorityAfter(const std::string &modulename)
{
	for (unsigned int j = 0; j < Instance->Config->module_names.size(); j++)
	{
		if (Instance->Config->module_names[j] == modulename)
		{
			return ((j << 8) | PRIORITY_AFTER);
		}
	}
	return PRIORITY_DONTCARE;
}

long ModuleManager::PriorityBefore(const std::string &modulename)
{
	for (unsigned int j = 0; j < Instance->Config->module_names.size(); j++)
	{
		if (Instance->Config->module_names[j] == modulename)
		{
			return ((j << 8) | PRIORITY_BEFORE);
		}
	}
	return PRIORITY_DONTCARE;
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
	static std::string nothing; /* Prevent compiler warning */

	if (!this->GetCount())
		return nothing;

	for (int i = 0; i <= this->GetCount(); i++)
	{
		if (this->modules[i] == m)
		{
			return Instance->Config->module_names[i];
		}
	}
	return nothing; /* As above */
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
	for (int i = 0; i <= this->GetCount(); i++)
	{
		if (Instance->Config->module_names[i] == name)
		{
			return this->modules[i];
		}
	}
	return NULL;
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
	this->readerror = ServerInstance->Config->LoadConf(*this->data, filename, *this->errorlog);
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
