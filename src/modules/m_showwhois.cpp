// showwhois module by typobox43

#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Allows opers to set +W to see when a user uses WHOIS on them */

Server *Srv;

class ModuleShowwhois : public Module {

	public:

		ModuleShowwhois() {

			Srv = new Server;

			Srv->AddExtendedMode('W',MT_CLIENT,true,0,0);

		}

		~ModuleShowwhois() {

			delete Srv;

		}

		virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list& params) {

			if((type == MT_CLIENT) && (modechar == 'W')) {

				return 1;

			}

			return 0;

		}

		virtual void OnWhois(userrec* source, userrec* dest) {

			if(strchr(dest->modes,'W')) {

				WriteServ(dest->fd,"NOTICE %s :*** %s (%s@%s) did a /whois on you.",dest->nick,source->nick,source->ident,source->host);
				
			}

		}

};

class ModuleShowwhoisFactory : public ModuleFactory {

	public:

		ModuleShowwhoisFactory() {

		}

		~ModuleShowwhoisFactory() {

		}

		virtual Module* CreateModule() {

			return new ModuleShowwhois;

		}

};

extern "C" void* init_module() {

	return new ModuleShowwhoisFactory;

}
