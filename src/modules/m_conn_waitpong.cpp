/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Forces connecting clients to send a PONG message back to the server before they can complete their connection */

class ModuleWaitPong : public Module
{
	bool sendsnotice;
	bool killonbadreply;
	const char* extenstr;

 public:
	ModuleWaitPong(InspIRCd* Me)
	 : Module(Me), extenstr("waitpong_pingstr")
	{
		OnRehash(NULL,"");
	}
	
	virtual void OnRehash(userrec* user, const std::string &param)
	{
		ConfigReader Conf(ServerInstance);
		
		sendsnotice = Conf.ReadFlag("waitpong", "sendsnotice", 0);
		
		if(Conf.GetError() == CONF_VALUE_NOT_FOUND)
			sendsnotice = true;
		
		killonbadreply = Conf.ReadFlag("waitpong", "killonbadreply", 0);

		if(Conf.GetError() == CONF_VALUE_NOT_FOUND)
			killonbadreply = true;
	}

	void Implements(char* List)
	{
		List[I_OnUserRegister] = List[I_OnCheckReady] = List[I_OnPreCommand] = List[I_OnRehash] = List[I_OnUserDisconnect] = List[I_OnCleanup] = 1;
	}

	char* RandString(unsigned int length)
	{
		unsigned char* out = new unsigned char[length+1];
		for(unsigned int i = 0; i < length; i++)
			out[i] = ((rand() % 26) + 65);
		out[length] = '\0';
	
		return (char*)out;
	}
	
	virtual int OnUserRegister(userrec* user)
	{
		char* pingrpl = RandString(10);
		
		user->Write("PING :%s", pingrpl);
		
		if(sendsnotice)
			user->WriteServ("NOTICE %s :*** If you are having problems connecting due to ping timeouts, please type /quote PONG %s or /raw PONG %s now.", user->nick, pingrpl, pingrpl);
			
		user->Extend(extenstr, pingrpl);
		return 0;
	}
	
	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec* user, bool validated, const std::string &original_line)
	{
		if(command == "PONG")
		{
			char* pingrpl;
			user->GetExt(extenstr, pingrpl);
			
			if(pingrpl)
			{
				if(strcmp(pingrpl, parameters[0]) == 0)
				{
					DELETE(pingrpl);
					user->Shrink(extenstr);
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
		return (!user->GetExt(extenstr, pingrpl));
	}
	
	virtual void OnUserDisconnect(userrec* user)
	{
		char* pingrpl;
		user->GetExt(extenstr, pingrpl);

		if(pingrpl)
		{
			DELETE(pingrpl);
			user->Shrink(extenstr);
		}
	}
	
	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			userrec* user = (userrec*)item;
			char* pingrpl;
			user->GetExt(extenstr, pingrpl);
			
			if(pingrpl)
			{
				DELETE(pingrpl);
				user->Shrink(extenstr);
			} 
		}
	}
	
	virtual ~ModuleWaitPong()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 1, VF_VENDOR, API_VERSION);
	}
	
};

MODULE_INIT(ModuleWaitPong)
