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
	public:
		ModuleOperLevels()
		{
			Implementation eventlist[] = { I_OnRehash, I_OnKill };
			ServerInstance->Modules->Attach(eventlist, this, 2);
		}

		virtual ~ModuleOperLevels()
		{
		}


		virtual void OnRehash(User* user)
		{
		}

		virtual Version GetVersion()
		{
			return Version("Gives each oper type a 'level', cannot kill opers 'above' your level.", VF_VENDOR);
		}

		virtual ModResult OnKill(User* source, User* dest, const std::string &reason)
		{
			// oper killing an oper?
			if (IS_OPER(dest) && IS_OPER(source))
			{
				TagIndex::iterator dest_type = ServerInstance->Config->opertypes.find(dest->oper);
				TagIndex::iterator src_type = ServerInstance->Config->opertypes.find(source->oper);

				if (dest_type == ServerInstance->Config->opertypes.end())
					return MOD_RES_PASSTHRU;
				if (src_type == ServerInstance->Config->opertypes.end())
					return MOD_RES_PASSTHRU;

				long dest_level = dest_type->second->getInt("level");
				long source_level = src_type->second->getInt("level");
				if (dest_level > source_level)
				{
					if (IS_LOCAL(source)) ServerInstance->SNO->WriteGlobalSno('a', "Oper %s (level %ld) attempted to /kill a higher oper: %s (level %ld): Reason: %s",source->nick.c_str(),source_level,dest->nick.c_str(),dest_level,reason.c_str());
					dest->WriteServ("NOTICE %s :*** Oper %s attempted to /kill you!",dest->nick.c_str(),source->nick.c_str());
					source->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - Oper %s is a higher level than you",source->nick.c_str(),dest->nick.c_str());
					return MOD_RES_DENY;
				}
			}
			return MOD_RES_PASSTHRU;
		}

};

MODULE_INIT(ModuleOperLevels)

