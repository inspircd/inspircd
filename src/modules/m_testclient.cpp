/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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
		Implementation eventlist[] = { I_OnBackgroundTimer };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	virtual Version GetVersion()
	{
		return Version("SQL test module", VF_VENDOR);
	}

	virtual void OnBackgroundTimer(time_t)
	{
		ServiceProvider* prov = ServerInstance->Modules->FindService(SERVICE_DATA, "SQL");
		if (!prov)
			return;
		Module* target = prov->creator;

		if(target)
		{
			SQLrequest foo = SQLrequest(this, target, "foo",
					SQLquery("UPDATE rawr SET foo = '?' WHERE bar = 42") % ServerInstance->Time());

			foo.Send();
			if (foo.cancel)
			{
				ServerInstance->Logs->Log("m_testclient.so", DEBUG, "SQLrequest failed: %s", foo.error.Str());
			}
			else
			{
				ServerInstance->Logs->Log("m_testclient.so", DEBUG, "Sent query, got given ID %lu", foo.id);
			}
		}
	}

	void OnRequest(Request& request)
	{
		if(strcmp(SQLRESID, request.id) == 0)
		{
			ServerInstance->Logs->Log("m_testclient.so", DEBUG, "Got SQL result (%s)", request.id);

			SQLresult* res = (SQLresult*)&request;

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
		}

		ServerInstance->Logs->Log("m_testclient.so", DEBUG, "Got unsupported API version string: %s", request.id);
	}

	virtual ~ModuleTestClient()
	{
	}
};

MODULE_INIT(ModuleTestClient)

