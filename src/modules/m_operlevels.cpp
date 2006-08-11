using namespace std;

#include "users.h"
#include "channels.h"
#include "modules.h"
#include <string>
#include "helperfuncs.h"
#include "inspircd.h"

/* $ModDesc: Gives each oper type a 'level', cannot kill opers 'above' your level. */



class ModuleOperLevels : public Module
{

	private:

		
		ConfigReader* conf;

	public:

		ModuleOperLevels(InspIRCd* Me)
			: Module::Module(Me)
		{

			
			conf = new ConfigReader(ServerInstance);
		}

		virtual ~ModuleOperLevels()
		{
			DELETE(conf);
		}

		void Implements(char* List)
		{
			List[I_OnRehash] = List[I_OnKill] = 1;
		}

		virtual void OnRehash(const std::string &parameter)
		{
			DELETE(conf);
			conf = new ConfigReader(ServerInstance);
		}

		virtual Version GetVersion()
		{
			return Version(1,0,0,1,VF_VENDOR);
		}

		virtual int OnKill(userrec* source, userrec* dest, const std::string &reason)
		{
			long dest_level = 0,source_level = 0;
			// oper killing an oper?
			if (*dest->oper)
			{
				for (int j =0; j < conf->Enumerate("type"); j++)
				{
					std::string typen = conf->ReadValue("type","name",j);
					if (!strcmp(typen.c_str(),dest->oper))
					{
						dest_level = conf->ReadInteger("type","level",j,true);
						break;
					}
				}
				for (int k =0; k < conf->Enumerate("type"); k++)
				{
					std::string typen = conf->ReadValue("type","name",k);
					if (!strcmp(typen.c_str(),source->oper))
					{
						source_level = conf->ReadInteger("type","level",k,true);
						break;
					}
				}
				if (dest_level > source_level)
				{
					ServerInstance->WriteOpers("Oper %s (level %d) attempted to /kill a higher oper: %s (level %d): Reason: %s",source->nick,source_level,dest->nick,dest_level,reason.c_str());
					dest->WriteServ("NOTICE %s :Oper %s attempted to /kill you!",dest->nick,source->nick);
					source->WriteServ("481 %s :Permission Denied- Oper %s is a higher level than you",source->nick,dest->nick);
					return 1;
				}
			}
			return 0;
		}

};

class ModuleOperLevelsFactory : public ModuleFactory
{
 public:
	ModuleOperLevelsFactory()
	{
	}

	~ModuleOperLevelsFactory()
	{
	}

	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleOperLevels(Me);
	}

};

extern "C" void * init_module( void )
{
	return new ModuleOperLevelsFactory;
}

