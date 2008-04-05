/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"

/* $ModDesc: Throttles the connections of any users who try quitflood the server */

class ModuleQuitBan : public Module
{
 private:
	clonemap quits;
	unsigned int threshold;
	unsigned int banduration;
 public:
	ModuleQuitBan(InspIRCd* Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnUserDisconnect, I_OnGarbageCollect, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 3);
		OnRehash(NULL, "");
	}
	
	virtual ~ModuleQuitBan()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,2,0,0,VF_VENDOR,API_VERSION);
	}

	virtual void OnRehash(User* user, const std::string &parameter)
	{
		ConfigReader Conf(ServerInstance);
		std::string duration;

		threshold = Conf.ReadInteger("quitban", "threshold", 0, true);

		if (threshold == 0)
			threshold = 10;

		duration = Conf.ReadValue("quitban", "duration", 0, true);

		if (duration.empty())
			duration = "10m";

		banduration = ServerInstance->Duration(duration);
	}

	virtual void OnUserDisconnect(User *u)
	{
		clonemap::iterator i = quits.find(u->GetIPString());

		if (i != quits.end())
		{
			i->second++;
			ServerInstance->Logs->Log("m_quitban",DEBUG, "quitban: Count for IP is now %d", i->second);

			if (i->second >= threshold)
			{
				// Create zline for set duration.
				ZLine* zl = new ZLine(ServerInstance, ServerInstance->Time(), banduration, ServerInstance->Config->ServerName, "Quit flooding", u->GetIPString());
				if (ServerInstance->XLines->AddLine(zl,NULL))
					ServerInstance->XLines->ApplyLines();
				else
					delete zl;

				ServerInstance->SNO->WriteToSnoMask('x', "Quit flooding from IP %s (%d)", u->GetIPString(), threshold);
				quits.erase(i);
			}
		}
		else
		{
			quits[u->GetIPString()] = 1;
			ServerInstance->Logs->Log("m_quitban",DEBUG, "quitban: Added new record");
		}
	}

	virtual void OnGarbageCollect()
	{
		ServerInstance->Logs->Log("m_quitban",DEBUG, "quitban: Clearing map.");
		quits.clear();
	}
};

MODULE_INIT(ModuleQuitBan)
