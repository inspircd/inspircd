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
#include "cull_list.h"
#include "exitcodes.h"

#ifndef WIN32
#include <dirent.h>
#endif

#ifndef PURE_STATIC

bool ModuleManager::Load(const std::string& filename, bool defer, ModuleState* state)
{
	/* Don't allow people to specify paths for modules, it doesn't work as expected */
	if (filename.find('/') != std::string::npos)
		return false;

	char modfile[MAXBUF];
	snprintf(modfile,MAXBUF,"%s/%s",ServerInstance->Config->ModPath.c_str(),filename.c_str());

	if (!ServerConfig::FileExists(modfile))
	{
		LastModuleError = "Module file could not be found: " + filename;
		ServerInstance->Logs->Log("MODULE", DEFAULT, LastModuleError);
		return false;
	}

	if (Modules.find(filename) != Modules.end())
	{
		LastModuleError = "Module " + filename + " is already loaded, cannot load a module twice!";
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
			newmod->ModuleSourceFile = filename;
			newmod->ModuleDLLManager = newhandle;
			Modules[filename] = newmod;
			if (defer)
			{
				Attach(I_ModuleInit, newmod);
				ServerInstance->Logs->Log("MODULE", DEFAULT,"New module introduced: %s (Module version %s)",
					filename.c_str(), newhandle->GetVersion().c_str());
			}
			else
			{
				newmod->early_init();

				ConfigReadStatus conf(REHASH_LOAD);
				newmod->ReadConfig(conf);

				if (conf.fatal)
					throw ModuleException(conf.errors.str());

				newmod->init();

				Version v = newmod->GetVersion();
				ServerInstance->Logs->Log("MODULE", DEFAULT,"New module introduced: %s (Module version %s)%s",
					filename.c_str(), newhandle->GetVersion().c_str(), (!(v.Flags & VF_VENDOR) ? " [3rd Party]" : " [Vendor]"));
			}
		}
		else
		{
			LastModuleError = "Unable to load " + filename + ": " + newhandle->LastError();
			ServerInstance->Logs->Log("MODULE", DEFAULT, LastModuleError);
			delete newhandle;
			return false;
		}
	}
	catch (CoreException& modexcept)
	{
		// failure in module constructor
		if (newmod)
			DoSafeUnload(newmod, NULL);
		else
			delete newhandle;
		LastModuleError = "Unable to load " + filename + ": " + modexcept.GetReason();
		ServerInstance->Logs->Log("MODULE", DEFAULT, LastModuleError);
		return false;
	}

	this->ModCount++;
	if (!defer)
		DoModuleLoad(newmod, state);
	return true;
}

namespace {
	struct UnloadAction : public HandlerBase0<void>
	{
		Module* const mod;
		UnloadAction(Module* m) : mod(m) {}
		void Call()
		{
			DLLManager* dll = mod->ModuleDLLManager;
			ServerInstance->Modules->DoSafeUnload(mod, NULL);
			ServerInstance->GlobalCulls->Apply();
			delete dll;
			ServerInstance->GlobalCulls->AddItem(this);
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
			ModuleState state;
			DLLManager* dll = mod->ModuleDLLManager;
			std::string name = mod->ModuleSourceFile;
			ServerInstance->Modules->DoSafeUnload(mod, &state);
			ServerInstance->GlobalCulls->Apply();
			delete dll;
			bool rv = ServerInstance->Modules->Load(name.c_str(), false, &state);
			if (callback)
				callback->Call(rv);
			ServerInstance->GlobalCulls->AddItem(this);
		}
	};
}

bool ModuleManager::Unload(Module* mod)
{
	if (!CanUnload(mod))
		return false;
	ServerInstance->AtomicActions->AddAction(new UnloadAction(mod));
	return true;
}

void ModuleManager::Reload(Module* mod, HandlerBase1<void, bool>* callback)
{
	if (CanUnload(mod))
		ServerInstance->AtomicActions->AddAction(new ReloadAction(mod, callback));
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

				if (!Load(entry->d_name, true))
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

	ConfigTagList tags = ServerInstance->Config->GetTags("module");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string name = tag->getString("name");
		printf_c("[\033[1;32m*\033[0m] Loading module:\t\033[1;32m%s\033[0m\n",name.c_str());

		if (!this->Load(name, true))
		{
			ServerInstance->Logs->Log("MODULE", DEFAULT, this->LastError());
			printf_c("\n[\033[1;31m*\033[0m] %s\n\n", this->LastError().c_str());
			ServerInstance->Exit(EXIT_STATUS_MODULE);
		}
	}

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
		{
			ServerInstance->Logs->Log("MODULE", DEFAULT, "Hook priority dependency loop detected");
			ServerInstance->Exit(EXIT_STATUS_MODULE);
		}
	}

	ConfigReadStatus conf(REHASH_BOOT);
	IntModuleList& initlist = EventHandlers[I_ModuleInit];
	for(size_t i=0; i < initlist.size(); i++)
	{
		Module* mod = initlist[i];
		try
		{
			ServerInstance->Logs->Log("MODULE", DEBUG, "Initializing %s", mod->ModuleSourceFile.c_str());
			mod->early_init();
			mod->ReadConfig(conf);
			mod->init();
		}
		catch (CoreException& modexcept)
		{
			LastModuleError = "Unable to initialize " + mod->ModuleSourceFile + ": " + modexcept.GetReason();
			ServerInstance->Logs->Log("MODULE", DEFAULT, LastModuleError);
			printf_c("\n[\033[1;31m*\033[0m] %s\n\n", LastModuleError.c_str());
			ServerInstance->Exit(EXIT_STATUS_MODULE);
		}
	}
	initlist.clear();
	if (conf.fatal)
	{
		fputs(conf.errors.str().c_str(), stdout); 
		ServerInstance->Exit(EXIT_STATUS_MODULE);
	}
}

#endif
