#include "inspircd.h"

#ifdef PURE_STATIC

static std::vector<AllCommandList::fn>* cmdlist = NULL;
static std::vector<AllModuleList*>* modlist = NULL;

AllCommandList::AllCommandList(fn cmd)
{
	if (!cmdlist)
		cmdlist = new std::vector<AllCommandList::fn>();
	cmdlist->push_back(cmd);
}

AllModuleList::AllModuleList(AllModuleList::fn mod, const std::string& Name) : init(mod), name(Name)
{
	if (!modlist)
		modlist = new std::vector<AllModuleList*>();
	modlist->push_back(this);
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

bool ModuleManager::Load(const char* filename)
{
	return false;
}

bool ModuleManager::CanUnload(Module* mod)
{
	return false;
}

void ModuleManager::DoSafeUnload(Module* mod)
{
}

bool ModuleManager::Unload(Module* mod)
{
	return false;
}

void ModuleManager::Reload(Module* mod, HandlerBase1<void, bool>* callback)
{
	callback->Call(false);
}

void ModuleManager::LoadAll()
{
	ModCount = 0;
	for(std::vector<AllModuleList*>::iterator i = modlist->begin(); i != modlist->end(); ++i)
	{
		try
		{
			Module* c = (*(**i).init)();
			c->ModuleSourceFile = (**i).name;
			Modules[(**i).name] = c;
			FOREACH_MOD(I_OnLoadModule,OnLoadModule(c));
		}
		catch (CoreException& modexcept)
		{
			ServerInstance->Logs->Log("MODULE", DEFAULT, "Unable to load " + (**i).name + ": " + modexcept.GetReason());
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
			ServerInstance->Logs->Log("MODULE", DEFAULT, "Hook priority dependency loop detected");
	}

	ServerInstance->BuildISupport();
}

void ModuleManager::UnloadAll()
{
	// TODO don't really need this, who cares if we leak on exit?
}

#endif
