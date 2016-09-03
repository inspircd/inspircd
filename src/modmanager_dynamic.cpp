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
#include "exitcodes.h"
#include <iostream>

#ifndef _WIN32
#include <dirent.h>
#endif

#ifndef INSPIRCD_STATIC

bool ModuleManager::Load(const std::string& modname, bool defer)
{
	/* Don't allow people to specify paths for modules, it doesn't work as expected */
	if (modname.find('/') != std::string::npos)
	{
		LastModuleError = "You can't load modules with a path: " + modname;
		return false;
	}

	const std::string filename = ExpandModName(modname);
	const std::string moduleFile = ServerInstance->Config->Paths.PrependModule(filename);

	if (!FileSystem::FileExists(moduleFile))
	{
		LastModuleError = "Module file could not be found: " + filename;
		ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, LastModuleError);
		return false;
	}

	if (Modules.find(filename) != Modules.end())
	{
		LastModuleError = "Module " + filename + " is already loaded, cannot load a module twice!";
		ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, LastModuleError);
		return false;
	}

	Module* newmod = NULL;
	DLLManager* newhandle = new DLLManager(moduleFile.c_str());
	ServiceList newservices;
	if (!defer)
		this->NewServices = &newservices;

	try
	{
		newmod = newhandle->CallInit();
		this->NewServices = NULL;

		if (newmod)
		{
			newmod->ModuleSourceFile = filename;
			newmod->ModuleDLLManager = newhandle;
			newmod->dying = false;
			Modules[filename] = newmod;
			std::string version = newhandle->GetVersion();
			if (defer)
			{
				ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, "New module introduced: %s (Module version %s)",
					filename.c_str(), version.c_str());
			}
			else
			{
				ConfigStatus confstatus;

				AttachAll(newmod);
				AddServices(newservices);
				newmod->init();
				newmod->ReadConfig(confstatus);

				Version v = newmod->GetVersion();
				ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, "New module introduced: %s (Module version %s)%s",
					filename.c_str(), version.c_str(), (!(v.Flags & VF_VENDOR) ? " [3rd Party]" : " [Vendor]"));
			}
		}
		else
		{
			LastModuleError = "Unable to load " + filename + ": " + newhandle->LastError();
			ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, LastModuleError);
			delete newhandle;
			return false;
		}
	}
	catch (CoreException& modexcept)
	{
		this->NewServices = NULL;

		// failure in module constructor
		if (newmod)
		{
			DoSafeUnload(newmod);
			ServerInstance->GlobalCulls.AddItem(newhandle);
		}
		else
			delete newhandle;
		LastModuleError = "Unable to load " + filename + ": " + modexcept.GetReason();
		ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, LastModuleError);
		return false;
	}

	if (defer)
		return true;

	FOREACH_MOD(OnLoadModule, (newmod));
	PrioritizeHooks();
	ServerInstance->ISupport.Build();
	return true;
}

/* We must load the modules AFTER initializing the socket engine, now */
void ModuleManager::LoadCoreModules(std::map<std::string, ServiceList>& servicemap)
{
	std::cout << std::endl << "Loading core commands";
	fflush(stdout);

	DIR* library = opendir(ServerInstance->Config->Paths.Module.c_str());
	if (library)
	{
		dirent* entry = NULL;
		while (0 != (entry = readdir(library)))
		{
			if (InspIRCd::Match(entry->d_name, "core_*.so", ascii_case_insensitive_map))
			{
				std::cout << ".";
				fflush(stdout);

				this->NewServices = &servicemap[entry->d_name];

				if (!Load(entry->d_name, true))
				{
					ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, this->LastError());
					std::cout << std::endl << "[" << con_red << "*" << con_reset << "] " << this->LastError() << std::endl << std::endl;
					ServerInstance->Exit(EXIT_STATUS_MODULE);
				}
			}
		}
		closedir(library);
		std::cout << std::endl;
	}
}

#endif
