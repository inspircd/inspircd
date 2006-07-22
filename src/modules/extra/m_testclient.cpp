#include <string>

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "inspircd.h"
#include "configreader.h"
#include "m_sqlv2.h"

class ModuleTestClient : public Module
{
private:
	Server* Srv;

public:
	ModuleTestClient(Server* Me)
	: Module::Module(Me), Srv(Me)
	{
	}

	void Implements(char* List)
	{
		List[I_OnRequest] = List[I_OnBackgroundTimer] = 1;
	}
		
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR);
	}
	
	virtual void OnBackgroundTimer(time_t foo)
	{
		Module* target = Srv->FindFeature("SQL");
		
		if(target)
		{
			SQLrequest foo = SQLreq(this, target, "foo", "SELECT foo, bar FROM ?", "rawr");
			
			if(foo.Send())
			{
				log(DEBUG, "Sent query, got given ID %lu", foo.id);
			}
			else
			{
				log(DEBUG, "SQLrequest failed: %s", foo.error.Str());
			}
		}
	}
	
	virtual char* OnRequest(Request* request)
	{
		if(strcmp(SQLRESID, request->GetData()) == 0)
		{
			log(DEBUG, "Got SQL result (%s)", request->GetData());
		
			SQLresult* res = (SQLresult*)request;

			if (res->error.Id() == NO_ERROR)
			{
				log(DEBUG, "Got result with %d rows and %d columns", res->Rows(), res->Cols());

				for (int r = 0; r < res->Rows(); r++)
				{
						log(DEBUG, "Row %d:", r);
					
					for(int i = 0; i < res->Cols(); i++)
					{
						log(DEBUG, "\t[%s]: %s", res->ColName(i).c_str(), res->GetValue(r, i).d.c_str());
					}
				}
			}
			else
			{
				log(DEBUG, "SQLrequest failed: %s", res->error.Str());
				
			}
		
			return SQLSUCCESS;
		}
		
		log(DEBUG, "Got unsupported API version string: %s", request->GetData());
		
		return NULL;
	}
	
	virtual ~ModuleTestClient()
	{
	}	
};

class ModuleTestClientFactory : public ModuleFactory
{
 public:
	ModuleTestClientFactory()
	{
	}
	
	~ModuleTestClientFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleTestClient(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModuleTestClientFactory;
}
