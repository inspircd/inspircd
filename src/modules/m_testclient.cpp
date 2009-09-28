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
#include "m_sqlv2.h"

class ModuleTestClient : public Module
{
private:


public:
	ModuleTestClient()
			{
		Implementation eventlist[] = { I_OnRequest, I_OnBackgroundTimer };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}


	virtual Version GetVersion()
	{
		return Version("Provides SSL support for clients", VF_VENDOR, API_VERSION);
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

