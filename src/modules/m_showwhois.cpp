using namespace std;

// showwhois module by typobox43

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Allows opers to set +W to see when a user uses WHOIS on them */

class ModuleShowwhois : public Module
{
		Server* Srv;

	public:
		ModuleShowwhois(Server* Me)
			: Module::Module(Me)
		{
			Srv = Me;
			Srv->AddExtendedMode('W',MT_CLIENT,true,0,0);
		}

		~ModuleShowwhois()
		{
		}

		void Implements(char* List)
		{
			List[I_OnWhois] = List[I_OnExtendedMode] = 1;
		}

		virtual Version GetVersion()
		{
			return Version(1,0,0,3,VF_STATIC);
		}

		virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list& params)
		{
			if((type == MT_CLIENT) && (modechar == 'W'))
			{
				return 1;
			}

			return 0;
		}

		virtual void OnWhois(userrec* source, userrec* dest)
		{
			if(strchr(dest->modes,'W'))
			{
				WriteServ(dest->fd,"NOTICE %s :*** %s (%s@%s) did a /whois on you.",dest->nick,source->nick,source->ident,source->host);
			}
		}

};

class ModuleShowwhoisFactory : public ModuleFactory
{
	public:
		ModuleShowwhoisFactory()
		{
		}

		~ModuleShowwhoisFactory()
		{
		}

		virtual Module* CreateModule(Server* Me)
		{
			return new ModuleShowwhois(Me);
		}

};

extern "C" void* init_module()
{
	return new ModuleShowwhoisFactory;
}
