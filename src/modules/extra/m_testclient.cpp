/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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
#include "m_sqlv2.h"

class ModuleTestClient : public Module
{
private:


public:
	ModuleTestClient(InspIRCd* Me)
		: Module(Me)
	{
		Implementation eventlist[] = { I_OnRequest, I_OnBackgroundTimer };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}


	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

	virtual void OnBackgroundTimer(time_t)
	{
		Module* target = ServerInstance->Modules->FindFeature("SQL");

		if(target)
		{
			SQLrequest foo = SQLrequest(this, target, "foo",
					SQLquery("UPDATE rawr SET foo = '?' WHERE bar = 42") % ServerInstance->Time());

			if(foo.Send())
			{
				ServerInstance->Logs->Log("m_testclient.so", DEBUG, "Sent query, got given ID %lu", foo.id);
			}
			else
			{
				ServerInstance->Logs->Log("m_testclient.so", DEBUG, "SQLrequest failed: %s", foo.error.Str());
			}
		}
	}

	virtual const char* OnRequest(Request* request)
	{
		if(strcmp(SQLRESID, request->GetId()) == 0)
		{
			ServerInstance->Logs->Log("m_testclient.so", DEBUG, "Got SQL result (%s)", request->GetId());

			SQLresult* res = (SQLresult*)request;

			if (res->error.Id() == SQL_NO_ERROR)
			{
				if(res->Cols())
				{
					ServerInstance->Logs->Log("m_testclient.so", DEBUG, "Got result with %d rows and %d columns", res->Rows(), res->Cols());

					for (int r = 0; r < res->Rows(); r++)
					{
						ServerInstance->Logs->Log("m_testclient.so", DEBUG, "Row %d:", r);

						for(int i = 0; i < res->Cols(); i++)
						{
							ServerInstance->Logs->Log("m_testclient.so", DEBUG, "\t[%s]: %s", res->ColName(i).c_str(), res->GetValue(r, i).d.c_str());
						}
					}
				}
				else
				{
					ServerInstance->Logs->Log("m_testclient.so", DEBUG, "%d rows affected in query", res->Rows());
				}
			}
			else
			{
				ServerInstance->Logs->Log("m_testclient.so", DEBUG, "SQLrequest failed: %s", res->error.Str());

			}

			return SQLSUCCESS;
		}

		ServerInstance->Logs->Log("m_testclient.so", DEBUG, "Got unsupported API version string: %s", request->GetId());

		return NULL;
	}

	virtual ~ModuleTestClient()
	{
	}
};

MODULE_INIT(ModuleTestClient)

