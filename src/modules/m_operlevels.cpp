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

/* $ModDesc: Gives each oper type a 'level', cannot kill opers 'above' your level. */
class ModuleOperLevels : public Module
{
	private:
		ConfigReader* conf;
	public:
		ModuleOperLevels(InspIRCd* Me)
			: Module(Me)
		{
			conf = new ConfigReader(ServerInstance);
			Implementation eventlist[] = { I_OnRehash, I_OnKill };
			ServerInstance->Modules->Attach(eventlist, this, 2);
		}

		virtual ~ModuleOperLevels()
		{
			delete conf;
		}


		virtual void OnRehash(User* user, const std::string &parameter)
		{
			delete conf;
			conf = new ConfigReader(ServerInstance);
		}

		virtual Version GetVersion()
		{
			return Version("$Id$", VF_VENDOR, API_VERSION);
		}

		virtual int OnKill(User* source, User* dest, const std::string &reason)
		{
			long dest_level = 0,source_level = 0;

			// oper killing an oper?
			if (IS_OPER(dest) && IS_OPER(source))
			{
				for (int j =0; j < conf->Enumerate("type"); j++)
				{
					std::string typen = conf->ReadValue("type","name",j);
					if (typen == dest->oper)
					{
						dest_level = conf->ReadInteger("type","level",j,true);
						break;
					}
				}
				for (int k =0; k < conf->Enumerate("type"); k++)
				{
					std::string typen = conf->ReadValue("type","name",k);
					if (typen == source->oper)
					{
						source_level = conf->ReadInteger("type","level",k,true);
						break;
					}
				}
				if (dest_level > source_level)
				{
					ServerInstance->SNO->WriteToSnoMask('a', "Oper %s (level %ld) attempted to /kill a higher oper: %s (level %ld): Reason: %s",source->nick.c_str(),source_level,dest->nick.c_str(),dest_level,reason.c_str());
					dest->WriteServ("NOTICE %s :*** Oper %s attempted to /kill you!",dest->nick.c_str(),source->nick.c_str());
					source->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - Oper %s is a higher level than you",source->nick.c_str(),dest->nick.c_str());
					return 1;
				}
			}
			return 0;
		}

};

MODULE_INIT(ModuleOperLevels)

