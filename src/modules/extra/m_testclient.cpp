#include <string>

#include "users.h"
#include "channels.h"
#include "modules.h"

#include "inspircd.h"
#include "configreader.h"
#include "m_sqlv2.h"



class ModuleTestClient : public Module
{
private:
	

public:
	ModuleTestClient(InspIRCd* Me)
		: Module::Module(Me)
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
		Module* target = ServerInstance->FindFeature("SQL");
		
		if(target)
		{
			SQLrequest foo = SQLreq(this, target, "foo", "UPDATE rawr SET foo = '?' WHERE bar = 42", ConvToStr(time(NULL)));
			
			if(foo.Send())
			{
				ServerInstance->Log(DEBUG, "Sent query, got given ID %lu", foo.id);
			}
			else
			{
				ServerInstance->Log(DEBUG, "SQLrequest failed: %s", foo.error.Str());
			}
		}
	}
	
	virtual char* OnRequest(Request* request)
	{
		if(strcmp(SQLRESID, request->GetId()) == 0)
		{
			ServerInstance->Log(DEBUG, "Got SQL result (%s)", request->GetId());
		
			SQLresult* res = (SQLresult*)request;

			if (res->error.Id() == NO_ERROR)
			{
				if(res->Cols())
				{
					ServerInstance->Log(DEBUG, "Got result with %d rows and %d columns", res->Rows(), res->Cols());

					for (int r = 0; r < res->Rows(); r++)
					{
						ServerInstance->Log(DEBUG, "Row %d:", r);
						
						for(int i = 0; i < res->Cols(); i++)
						{
							ServerInstance->Log(DEBUG, "\t[%s]: %s", res->ColName(i).c_str(), res->GetValue(r, i).d.c_str());
						}
					}
				}
				else
				{
					ServerInstance->Log(DEBUG, "%d rows affected in query", res->Rows());
				}
			}
			else
			{
				ServerInstance->Log(DEBUG, "SQLrequest failed: %s", res->error.Str());
				
			}
		
			return SQLSUCCESS;
		}
		
		ServerInstance->Log(DEBUG, "Got unsupported API version string: %s", request->GetId());
		
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleTestClient(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModuleTestClientFactory;
}
