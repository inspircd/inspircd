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
#include "socket.h"
#include "socketengine.h"
#include "command_parse.h"
#include "dns.h"
#include "exitcodes.h"

#ifndef WIN32
#include <dirent.h>
#endif

#ifndef PURE_STATIC

bool ModuleManager::Load(const char* filename)
{
	/* Don't allow people to specify paths for modules, it doesn't work as expected */
	if (strchr(filename, '/'))
		return false;
	/* Do we have a glob pattern in the filename?
	 * The user wants to load multiple modules which
	 * match the pattern.
	 */
	if (strchr(filename,'*') || (strchr(filename,'?')))
	{
		int n_match = 0;
		DIR* library = opendir(ServerInstance->Config->ModPath.c_str());
		if (library)
		{
			/* Try and locate and load all modules matching the pattern */
			dirent* entry = NULL;
			while (0 != (entry = readdir(library)))
			{
				if (InspIRCd::Match(entry->d_name, filename, ascii_case_insensitive_map))
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
		return (n_match > 0 ? false : true);
	}

	char modfile[MAXBUF];
	snprintf(modfile,MAXBUF,"%s/%s",ServerInstance->Config->ModPath.c_str(),filename);
	std::string filename_str = filename;

	if (!ServerConfig::FileExists(modfile))
	{
		LastModuleError = "Module file could not be found: " + filename_str;
		ServerInstance->Logs->Log("MODULE", DEFAULT, LastModuleError);
		return false;
	}

	if (Modules.find(filename_str) != Modules.end())
	{
		LastModuleError = "Module " + filename_str + " is already loaded, cannot load a module twice!";
		ServerInstance->Logs->Log("MODULE", DEFAULT, LastModuleError);
		return false;
	}

	Module* newmod = NULL;
	DLLManager* newhandle = new DLLManager(modfile);

	try
	{
		newmod = newhandle->CallInit();

		if (newmod)
		{
			newmod->ModuleSourceFile = filename_str;
			newmod->ModuleDLLManager = newhandle;
			Version v = newmod->GetVersion();

			ServerInstance->Logs->Log("MODULE", DEFAULT,"New module introduced: %s (Module version %s)%s",
				filename, newhandle->GetVersion().c_str(), (!(v.Flags & VF_VENDOR) ? " [3rd Party]" : " [Vendor]"));

			Modules[filename_str] = newmod;
		}
		else
		{
			LastModuleError = "Unable to load " + filename_str + ": " + newhandle->LastError();
			ServerInstance->Logs->Log("MODULE", DEFAULT, LastModuleError);
			delete newhandle;
			return false;
		}
	}
	catch (CoreException& modexcept)
	{
		// failure in module constructor
		delete newmod;
		delete newhandle;
		LastModuleError = "Unable to load " + filename_str + ": " + modexcept.GetReason();
		ServerInstance->Logs->Log("MODULE", DEFAULT, LastModuleError);
		return false;
	}

	this->ModCount++;
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
			ServerInstance->Logs->Log("MODULE", DEFAULT, "Hook priority dependency loop detected while loading " + filename_str);
	}

	ServerInstance->BuildISupport();
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

void ModuleManager::DoSafeUnload(Module* mod)
{
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
	for(unsigned int i = 0; i < ServerInstance->Modules->ActiveDynrefs.size(); i++)
		ServerInstance->Modules->ActiveDynrefs[i]->ClearCache();

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

namespace {
	struct UnloadAction : public HandlerBase0<void>
	{
		Module* const mod;
		UnloadAction(Module* m) : mod(m) {}
		void Call()
		{
			DLLManager* dll = mod->ModuleDLLManager;
			ServerInstance->Modules->DoSafeUnload(mod);
			ServerInstance->GlobalCulls.Apply();
			delete dll;
			ServerInstance->GlobalCulls.AddItem(this);
		}
	};

	struct ReloadAction : public HandlerBase0<void>
	{
		Module* const mod;
		HandlerBase1<void, bool>* const callback;
		ReloadAction(Module* m, HandlerBase1<void, bool>* c)
			: mod(m), callback(c) {}
		void Call()
		{
			DLLManager* dll = mod->ModuleDLLManager;
			std::string name = mod->ModuleSourceFile;
			ServerInstance->Modules->DoSafeUnload(mod);
			ServerInstance->GlobalCulls.Apply();
			delete dll;
			bool rv = ServerInstance->Modules->Load(name.c_str());
			callback->Call(rv);
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

void ModuleManager::Reload(Module* mod, HandlerBase1<void, bool>* callback)
{
	if (CanUnload(mod))
		ServerInstance->AtomicActions.AddAction(new ReloadAction(mod, callback));
	else
		callback->Call(false);
}

/* We must load the modules AFTER initializing the socket engine, now */
void ModuleManager::LoadAll()
{
	ModCount = 0;

	printf("\nLoading core commands");
	fflush(stdout);

	DIR* library = opendir(ServerInstance->Config->ModPath.c_str());
	if (library)
	{
		dirent* entry = NULL;
		while (0 != (entry = readdir(library)))
		{
			if (InspIRCd::Match(entry->d_name, "cmd_*.so", ascii_case_insensitive_map))
			{
				printf(".");
				fflush(stdout);

				if (!Load(entry->d_name))
				{
					ServerInstance->Logs->Log("MODULE", DEFAULT, this->LastError());
					printf_c("\n[\033[1;31m*\033[0m] %s\n\n", this->LastError().c_str());
					ServerInstance->Exit(EXIT_STATUS_MODULE);
				}
			}
		}
		closedir(library);
		printf("\n");
	}

	ConfigTagList tags = ServerInstance->Config->ConfTags("module");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string name = tag->getString("name");
		printf_c("[\033[1;32m*\033[0m] Loading module:\t\033[1;32m%s\033[0m\n",name.c_str());

		if (!this->Load(name.c_str()))
		{
			ServerInstance->Logs->Log("MODULE", DEFAULT, this->LastError());
			printf_c("\n[\033[1;31m*\033[0m] %s\n\n", this->LastError().c_str());
			ServerInstance->Exit(EXIT_STATUS_MODULE);
		}
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
				DoSafeUnload(me->second);
			}
		}
		ServerInstance->GlobalCulls.Apply();
	}
}

#endif
