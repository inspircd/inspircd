#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"
#include "httpclient.h"

/* $ModDep: httpclient.h */

class MyModule : public Module
{
public:
       MyModule(InspIRCd* Me)
               : Module::Module(Me)
       {
       }

       virtual ~MyModule()
       {
       }

       virtual void Implements(char* List)
       {
		   List[I_OnUserJoin] = List[I_OnUserPart] = 1;
       }

       virtual Version GetVersion()
       {
               return Version(1,0,0,1,0,API_VERSION);
       }

       virtual void OnUserJoin(userrec* user, chanrec* channel)
       {
               // method called when a user joins a channel

               std::string chan = channel->name;
               std::string nick = user->nick;
//               ServerInstance->Log(DEBUG,"User " + nick + " joined " + chan);

			   Module* target = ServerInstance->FindModule("m_http_client");
			   if(target) {
				   HTTPClientRequest req(ServerInstance, this, target, "http://znc.in/~psychon");
				   req.Send();
			   }
			   else
				   ServerInstance->Log(DEBUG,"module not found, load it!!");
       }

	   char* OnRequest(Request* req)
	   {
		   HTTPClientResponse* resp = (HTTPClientResponse*)req;
		   if(!strcmp(resp->GetId(), HTTP_CLIENT_RESPONSE))
		   {
			  ServerInstance->Log(DEBUG, resp->GetData()); 
		   }
		   return NULL;
	   }

       virtual void OnUserPart(userrec* user, chanrec* channel, const std::string &partmessage)
       {
       }

};

class MyModuleFactory : public ModuleFactory
{
public:
       MyModuleFactory()
       {
       }

       ~MyModuleFactory()
       {
       }

       virtual Module * CreateModule(InspIRCd* Me)
       {
               return new MyModule(Me);
       }

};

extern "C" void * init_module( void )
{
       return new MyModuleFactory;
}

