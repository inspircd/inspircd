/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "rpc.h"

/* $ModDesc: A test of the RPC API */
/* $ModDep: rpc.h */

class ModuleRPCTest : public Module
{
 private:

 public:
	ModuleRPCTest(InspIRCd *Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnEvent };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	virtual ~ModuleRPCTest()
	{
	}

	virtual Version GetVersion()
	{
		return Version("A test of the RPC API", VF_VENDOR, API_VERSION);
	}


	virtual void OnEvent(Event *ev)
	{
		if (ev->GetEventID() == "RPCMethod")
		{
			RPCRequest *req = (RPCRequest*) ev->GetData();

			if (req->method == "test.echo")
			{
				req->claimed = true;
				if (req->parameters->ArraySize() < 1)
				{
					req->error = "Insufficient parameters";
					return;
				}

				req->result->SetString(req->parameters->GetArray(0)->GetString());
			}
		}
	}
};

MODULE_INIT(ModuleRPCTest)

