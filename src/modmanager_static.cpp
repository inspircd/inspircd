#define MODNAME AllModule

#include "inspircd.h"
#include "exitcodes.h"

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

void ModuleManager::LoadAll()
{
	Load("AllModule", true);

	ConfigTagList tags = ServerInstance->Config->ConfTags("module");
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
			printf_c("\n[\033[1;31m*\033[0m] %s\n\n", LastModuleError.c_str());
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
