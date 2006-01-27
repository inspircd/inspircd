#include <stdlib.h>
#include <string>
#include "aes.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Forces connecting clients to send a PONG message back to the server before they can complete their connection */

static std::string RandString(unsigned int length)
{
	unsigned char* tmp = new unsigned char[(length/4)*3];
	
	for(unsigned int i = 0; i < (length/4)*3; i++)
		tmp[i] = (unsigned char)rand();
		
	unsigned char* out = new unsigned char[length];	

	to64frombits(out, tmp, (length/4)*3);
	
	std::string ret((char*)out);

	delete out;
	delete tmp;
	
	return ret;
}

class ModuleWaitPong : public Module
{
	Server* Srv;
	ConfigReader* Conf;
	
	bool sendsnotice;
	bool killonbadreply;

 public:
	ModuleWaitPong(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		OnRehash("");
	}
	
	virtual void OnRehash(std::string param)
	{
		Conf = new ConfigReader;
		
		sendsnotice = Conf->ReadFlag("waitpong", "sendsnotice", 0);
		
		if(Conf->GetError() == CONF_VALUE_NOT_FOUND)
			sendsnotice = true;
		
		killonbadreply = Conf->ReadFlag("waitpong", "killonbadreply", 0);

		if(Conf->GetError() == CONF_VALUE_NOT_FOUND)
			killonbadreply = true;
				
		delete Conf;
	}

	void Implements(char* List)
	{
		List[I_OnUserRegister] = List[I_OnCheckReady] = List[I_OnPreCommand] = List[I_OnRehash] = 1;
	}

	virtual void OnUserRegister(userrec* user)
	{
		std::string* pingrpl = new std::string;
		*pingrpl = RandString(10);
		
		Srv->Send(user->fd, "PING :" + *pingrpl);
		
		if(sendsnotice)
			WriteServ(user->fd, "NOTICE %s :*** If you are having problems connecting due to ping timeouts, please type /quote PONG %s or /raw PONG %s now.", user->nick, pingrpl->c_str(), pingrpl->c_str());
			
		user->Extend("waitpong_pingstr", (char*)pingrpl);
	}
	
	virtual int OnPreCommand(std::string command, char** parameters, int pcnt,	userrec* user,	bool validated)
	{
		if(command == "PONG")
		{
			std::string* pingrpl = (std::string*)user->GetExt("waitpong_pingstr");
			
			if(pingrpl && (*pingrpl == parameters[0]))
			{
				delete pingrpl;
				user->Shrink("waitpong_pingstr");
				return 1;
			}
			else if(killonbadreply)
			{
				Srv->QuitUser(user, "Incorrect ping reply for registration");
				return 1;
			}
		}
		
		return 0;
	}

	virtual bool OnCheckReady(userrec* user)
	{
		return (!user->GetExt("waitpong_pingstr"));
	}
		
	virtual ~ModuleWaitPong()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR);
	}
	
};

class ModuleWaitPongFactory : public ModuleFactory
{
 public:
	ModuleWaitPongFactory()
	{
	}
	
	~ModuleWaitPongFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleWaitPong(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModuleWaitPongFactory;
}
