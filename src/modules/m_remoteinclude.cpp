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
#include "httpclient.h"

/* $ModDesc: The base module for remote includes */

class ModuleRemoteInclude : public Module
{
	std::map<std::string, std::stringstream*> assoc;

 public:
	ModuleRemoteInclude(InspIRCd* Me)
		: Module(Me)
	{
		ServerInstance->Modules->Attach(I_OnDownloadFile, this);
		ServerInstance->Modules->Attach(I_OnRequest, this);
	}
	
	virtual ~ModuleRemoteInclude()
	{
	}
	
	virtual Version GetVersion()
	{
		// this method instantiates a class of type Version, and returns
		// the modules version information using it.
	
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}

	char* OnRequest(Request* req)
	{
		if (!strcmp(req->GetId(), HTTP_CLIENT_RESPONSE))
		{
			HTTPClientResponse* resp = (HTTPClientResponse*)req;
			ServerInstance->Log(DEBUG, "Got http file for %s", resp->GetURL().c_str());

			std::map<std::string, std::stringstream*>::iterator n = assoc.find(resp->GetURL());

			if (n == assoc.end())
				ServerInstance->Config->Complete(resp->GetURL(), true);
		
			std::string responsestr;
			if (resp->GetResponse(responsestr) == 200)
			{
				*(n->second) << resp->GetData();

				ServerInstance->Log(DEBUG, "Got data: %s", resp->GetData().c_str());

				ServerInstance->Log(DEBUG, "Flag file complete without error");
				ServerInstance->Config->Complete(resp->GetURL(), false);
			}
			else
				ServerInstance->Config->Complete(resp->GetURL(), true);

			/* Erase from our association map, but dont delete the pointer.
			 * the core will want to access this pointer for the file data.
			 */
			assoc.erase(n);
		}
		else if (!strcmp(req->GetId(), HTTP_CLIENT_ERROR))
		{
			HTTPClientError* resp = (HTTPClientError*)req;

			ServerInstance->Log(DEBUG, "Got http error when accessing %s", resp->GetURL().c_str());
			ServerInstance->Config->Complete(resp->GetURL(), true);

			std::map<std::string, std::stringstream*>::iterator n = assoc.find(resp->GetURL());

			if (n != assoc.end())
				assoc.erase(n);
		}
		return NULL;
	}

	int OnDownloadFile(const std::string &name, std::istream* &filedata)
	{
		if (name.substr(0, 7) == "http://")
		{
			Module* target = ServerInstance->Modules->Find("m_http_client.so");
			if (target)
			{
				ServerInstance->Log(DEBUG,"Claiming schema http://, making fetch request");

				HTTPClientRequest req(ServerInstance, this, target, name);
				req.Send();

				assoc[name] = new std::stringstream();
				delete filedata;
				filedata = assoc[name];

				return true;
			}
		}

		return false;
	}
};


MODULE_INIT(ModuleRemoteInclude)

