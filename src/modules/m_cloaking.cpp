// Hostname cloaking (+x mode) module for inspircd.
// version 1.0.0.1 by brain (C. J. Edwards) Mar 2004.
//
// When loaded this module will automatically set the
// +x mode on all connecting clients.
//
// Setting +x on a client causes the module to change the
// dhost entry (displayed host) for each user who has the
// mode, cloaking their host. Unlike unreal, the algorithm
// is non-reversible as uncloaked hosts are passed along
// the server->server link, and all encoding of hosts is
// done locally on the server by this module.

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides masking of user hostnames */

class ModuleCloaking : public Module
{
 private:

	 Server *Srv;
	 
 public:
	ModuleCloaking()
	{
		// We must create an instance of the Server class to work with
		Srv = new Server;
		
		// we must create a new mode. Set the parameters so the
		// mode doesn't require oper, and is a client usermode
  		// with no parameters (actually, you cant have params for a umode!)
		if (!Srv->AddExtendedMode('x',MT_CLIENT,false,0,0))
		{
			// we couldn't claim mode x... possibly anther module has it,
			// this might become likely to happen if there are a lot of 3rd
			// party modules around in the future -- any 3rd party modules
			// SHOULD implement a system of configurable mode letters (e.g.
			// from a config file)
			Srv->Log(DEFAULT,"*** m_cloaking: ERROR, failed to allocate user mode +x!");
			printf("Could not claim usermode +x for this module!");
			exit(0);
		}
	}
	
	virtual ~ModuleCloaking()
	{
		// not really neccessary, but free it anyway
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		// returns the version number of the module to be
		// listed in /MODULES
		return Version(1,0,0,1);
	}
	
	virtual bool OnExtendedMode(userrec* user, chanrec* chan, char modechar, int type, bool mode_on, string_list &params)
	{
		// this method is called for any extended mode character.
		// all module modes for all modules pass through here
		// (unless another module further up the chain claims them)
		// so we must be VERY careful to only act upon modes which
		// we have claimed ourselves. This is a feature to allow
		// modules to 'spy' on extended mode activity if they so wish.
		if ((modechar == 'x') && (type == MT_CLIENT))
  		{
			// we've now determined that this is our mode character...
			// is the user adding the mode to their list or removing it?
			if (mode_on)
			{
				// the mode is being turned on - so attempt to
				// allocate the user a cloaked host using a non-reversible
				// algorithm (its simple, but its non-reversible so the
				// simplicity doesnt really matter). This algorithm
				// will not work if the user has only one level of domain
				// naming in their hostname (e.g. if they are on a lan or
				// are connecting via localhost) -- this doesnt matter much.
				if (strstr(user->host,"."))
				{
					// in inspircd users have two hostnames. A displayed
					// hostname which can be modified by modules (e.g.
					// to create vhosts, implement chghost, etc) and a
					// 'real' hostname which you shouldnt write to.
					std::string a = strstr(user->host,".");
					char ra[64];
					long seed,s2;
					memcpy(&seed,user->host,sizeof(long));
					memcpy(&s2,a.c_str(),sizeof(long));
					sprintf(ra,"%.8X",seed*s2*strlen(user->host));
					std::string b = Srv->GetNetworkName() + "-" + ra + a;
					Srv->Log(DEBUG,"cloak: allocated "+b);
					strcpy(user->dhost,b.c_str());
				}
			}
			else
  			{
  				// user is removing the mode, so just restore their real host
  				// and make it match the displayed one.
  				strcpy(user->dhost,user->host);
			}
			// this mode IS ours, and we have handled it. If we chose not to handle it,
			// for example the user cannot cloak as they have a vhost or such, then
			// we could return 0 here instead of 1 and the core would not send the mode
			// change to the user.
			return 1;
		}
		else
		{
			// this mode isn't ours, we have to bail and return 0 to not handle it.
			return 0;
		}
	}

	virtual void OnUserConnect(userrec* user)
	{
		// Heres the weird bit. When a user connects we must set +x on them, so
		// we're going to use the SendMode method of the Server class to send
		// the mode to the client. This is basically the same as sending an
		// SAMODE in unreal. Note that to the user it will appear as if they set
		// the mode on themselves.
		
		char* modes[2];			// only two parameters
		modes[0] = user->nick;		// first parameter is the nick
		modes[1] = "+x";		// second parameter is the mode
		Srv->SendMode(modes,2,user);	// send these, forming the command "MODE <nick> +x"
	}

};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleCloakingFactory : public ModuleFactory
{
 public:
	ModuleCloakingFactory()
	{
	}
	
	~ModuleCloakingFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleCloaking;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleCloakingFactory;
}

