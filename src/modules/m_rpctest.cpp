/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 John Brooks <john.brooks@dereferenced.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
		return Version("$Id$", VF_VENDOR, API_VERSION);
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

