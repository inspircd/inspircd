/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include <iostream>

#ifndef _WIN32
#include <dirent.h>
#endif

#ifndef PURE_STATIC

bool ModuleManager::Load(const std::string& filename, bool defer)
{
	/* Don't allow people to specify paths for modules, it doesn't work as expected */
	if (filename.find('/') != std::string::npos)
	{
		LastModuleError = "You can't load modules with a path: " + filename;
		return false;
	}

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
			newmod->dying = false;
			Modules[filename] = newmod;
			std::string version = newhandle->GetVersion();
			if (defer)
			{
				ServerInstance->Logs->Log("MODULE", DEFAULT,"New module introduced: %s (Module version %s)",
					filename.c_str(), version.c_str());
			}
			else
			{
				newmod->init();

				Version v = newmod->GetVersion();
				ServerInstance->Logs->Log("MODULE", DEFAULT,"New module introduced: %s (Module version %s)%s",
					filename.c_str(), version.c_str(), (!(v.Flags & VF_VENDOR) ? " [3rd Party]" : " [Vendor]"));
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
		{
			DoSafeUnload(newmod);
			ServerInstance->GlobalCulls.AddItem(newhandle);
		}
		else
			delete newhandle;
		LastModuleError = "Unable to load " + filename + ": " + modexcept.GetReason();
		ServerInstance->Logs->Log("MODULE", DEFAULT, LastModuleError);
		return false;
	}

	this->ModCount++;
	if (defer)
		return true;

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
			ServerInstance->Logs->Log("MODULE", DEFAULT, "Hook priority dependency loop detected while loading " + filename);
	}

	ServerInstance->BuildISupport();
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
			if (callback)
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
	else if (callback)
		callback->Call(false);
}

/* We must load the modules AFTER initializing the socket engine, now */
void ModuleManager::LoadAll()
{
	ModCount = 0;

	std::cout << std::endl << "Loading core commands";
	fflush(stdout);

	DIR* library = opendir(ServerInstance->Config->ModPath.c_str());
	if (library)
	{
		dirent* entry = NULL;
		while (0 != (entry = readdir(library)))
		{
			if (InspIRCd::Match(entry->d_name, "cmd_*.so", ascii_case_insensitive_map))
			{
				std::cout << ".";
				fflush(stdout);

				if (!Load(entry->d_name, true))
				{
					ServerInstance->Logs->Log("MODULE", DEFAULT, this->LastError());
					std::cout << std::endl << "[" << con_red << "*" << con_reset << "] " << this->LastError() << std::endl << std::endl;
					ServerInstance->Exit(EXIT_STATUS_MODULE);
				}
			}
		}
		closedir(library);
		std::cout << std::endl;
	}

	ConfigTagList tags = ServerInstance->Config->ConfTags("module");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string name = tag->getString("name");
		std::cout << "[" << con_green << "*" << con_reset << "] Loading module:\t" << con_green << name << con_reset << std::endl;

		if (!this->Load(name, true))
		{
			ServerInstance->Logs->Log("MODULE", DEFAULT, this->LastError());
			std::cout << std::endl << "[" << con_red << "*" << con_reset << "] " << this->LastError() << std::endl << std::endl;
			ServerInstance->Exit(EXIT_STATUS_MODULE);
		}
	}

	for(std::map<std::string, Module*>::iterator i = Modules.begin(); i != Modules.end(); i++)
	{
		Module* mod = i->second;
		try
		{
			ServerInstance->Logs->Log("MODULE", DEBUG, "Initializing %s", i->first.c_str());
			mod->init();
		}
		catch (CoreException& modexcept)
		{
			LastModuleError = "Unable to initialize " + mod->ModuleSourceFile + ": " + modexcept.GetReason();
			ServerInstance->Logs->Log("MODULE", DEFAULT, LastModuleError);
			std::cout << std::endl << "[" << con_red << "*" << con_reset << "] " << LastModuleError << std::endl << std::endl;
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
}

#endif
