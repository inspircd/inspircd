#include <stdlib.h>
#include <string>
#include "aes.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

#include "inspircd.h"

/* $ModDesc: Forces connecting clients to send a PONG message back to the server before they can complete their connection */



char* RandString(unsigned int length)
{
	unsigned char* tmp = new unsigned char[(length/4)*3];
	
	for(unsigned int i = 0; i < (length/4)*3; i++)
		tmp[i] = (unsigned char)rand();
		
	unsigned char* out = new unsigned char[length+1];

	to64frombits(out, tmp, (length/4)*3);
	
	out[length] = '\0';
	
	DELETE(tmp);
	
	return (char*)out;
}

class ModuleWaitPong : public Module
{
	
	ConfigReader* Conf;
	
	bool sendsnotice;
	bool killonbadreply;

 public:
	ModuleWaitPong(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		OnRehash("");
	}
	
	virtual void OnRehash(const std::string &param)
	{
		Conf = new ConfigReader(ServerInstance);
		
		sendsnotice = Conf->ReadFlag("waitpong", "sendsnotice", 0);
		
		if(Conf->GetError() == CONF_VALUE_NOT_FOUND)
			sendsnotice = true;
		
		killonbadreply = Conf->ReadFlag("waitpong", "killonbadreply", 0);

		if(Conf->GetError() == CONF_VALUE_NOT_FOUND)
			killonbadreply = true;
				
		DELETE(Conf);
	}

	void Implements(char* List)
	{
		List[I_OnUserRegister] = List[I_OnCheckReady] = List[I_OnPreCommand] = List[I_OnRehash] = List[I_OnUserDisconnect] = List[I_OnCleanup] = 1;
	}

	virtual void OnUserRegister(userrec* user)
	{
		char* pingrpl = RandString(10);
		
		user->Write("PING :%s", pingrpl);
		
		if(sendsnotice)
			user->WriteServ("NOTICE %s :*** If you are having problems connecting due to ping timeouts, please type /quote PONG %s or /raw PONG %s now.", user->nick, pingrpl, pingrpl);
			
		user->Extend("waitpong_pingstr", pingrpl);
	}
	
	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec* user, bool validated)
	{
		if(command == "PONG")
		{
			char* pingrpl;
			user->GetExt("waitpong_pingstr", pingrpl);
			
			if(pingrpl)
			{
				if(strcmp(pingrpl, parameters[0]) == 0)
				{
					DELETE(pingrpl);
					user->Shrink("waitpong_pingstr");
					return 1;
				}
				else
				{
					if(killonbadreply)
						userrec::QuitUser(ServerInstance, user, "Incorrect ping reply for registration");
					return 1;
				}
			}
		}
		
		return 0;
	}

	virtual bool OnCheckReady(userrec* user)
	{
		char* pingrpl;
		return (!user->GetExt("waitpong_pingstr", pingrpl));
	}
	
	virtual void OnUserDisconnect(userrec* user)
	{
		char* pingrpl;
		user->GetExt("waitpong_pingstr", pingrpl);

		if(pingrpl)
		{
			DELETE(pingrpl);
			user->Shrink("waitpong_pingstr");
		}
	}
	
	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			userrec* user = (userrec*)item;
			char* pingrpl;
			user->GetExt("waitpong_pingstr", pingrpl);
			
			if(pingrpl)
			{
				DELETE(pingrpl);
				user->Shrink("waitpong_pingstr");
			} 
		}
	}
	
	virtual ~ModuleWaitPong()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 1, VF_VENDOR);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleWaitPong(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModuleWaitPongFactory;
}
