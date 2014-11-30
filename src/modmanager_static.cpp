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


#define MODNAME cmd_all

#include "inspircd.h"
#include "exitcodes.h"
#include <iostream>

#ifdef PURE_STATIC

typedef std::map<std::string, AllModuleList*> modmap;
static std::vector<AllCommandList::fn>* cmdlist = NULL;
static modmap* modlist = NULL;

AllCommandList::AllCommandList(fn cmd)
{
	if (!cmdlist)
		cmdlist = new std::vector<AllCommandList::fn>();
	cmdlist->push_back(cmd);
}

AllModuleList::AllModuleList(AllModuleList::fn mod, const std::string& Name) : init(mod), name(Name)
{
	if (!modlist)
		modlist = new modmap();
	modlist->insert(std::make_pair(Name, this));
}

class AllModule : public Module
{
	std::vector<Command*> cmds;
 public:
	AllModule()
	{
		if (!cmdlist)
			return;
		try
		{
			cmds.reserve(cmdlist->size());
			for(std::vector<AllCommandList::fn>::iterator i = cmdlist->begin(); i != cmdlist->end(); ++i)
			{
				Command* c = (*i)(this);
				cmds.push_back(c);
				ServerInstance->AddCommand(c);
			}
		}
		catch (...)
		{
			this->AllModule::~AllModule();
			throw;
		}
	}

	~AllModule()
	{
		for(std::vector<Command*>::iterator i = cmds.begin(); i != cmds.end(); ++i)
			delete *i;
	}

	Version GetVersion()
	{
		return Version("All commands", VF_VENDOR|VF_CORE);
	}
};

MODULE_INIT(AllModule)

bool ModuleManager::Load(const std::string& name, bool defer)
{
	modmap::iterator it = modlist->find(name);
	if (it == modlist->end())
		return false;
	Module* mod = NULL;
	try
	{
		mod = (*it->second->init)();
		mod->ModuleSourceFile = name;
		mod->ModuleDLLManager = NULL;
		mod->dying = false;
		Modules[name] = mod;
		if (defer)
		{
			ServerInstance->Logs->Log("MODULE", DEFAULT,"New module introduced: %s", name.c_str());
			return true;
		}
		else
		{
			mod->init();
		}
	}
	catch (CoreException& modexcept)
	{
		if (mod)
			DoSafeUnload(mod);
		ServerInstance->Logs->Log("MODULE", DEFAULT, "Unable to load " + name + ": " + modexcept.GetReason());
		return false;
	}
	FOREACH_MOD(I_OnLoadModule,OnLoadModule(mod));
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
			ServerInstance->Logs->Log("MODULE", DEFAULT, "Hook priority dependency loop detected while loading " + name);
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
			ServerInstance->Modules->DoSafeUnload(mod);
			ServerInstance->GlobalCulls.Apply();
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
			std::string name = mod->ModuleSourceFile;
			ServerInstance->Modules->DoSafeUnload(mod);
			ServerInstance->GlobalCulls.Apply();
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

void ModuleManager::LoadAll()
{
	Load("cmd_all", true);
	Load("cmd_whowas.so", true);
	Load("cmd_lusers.so", true);

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
