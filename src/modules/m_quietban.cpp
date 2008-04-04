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

/* $ModDesc: Implements extban +b q: - quiet bans */

class ModuleQuietBan : public Module
{
 private:
 public:
	ModuleQuietBan(InspIRCd* Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}
	
	virtual ~ModuleQuietBan()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,2,0,0,VF_VENDOR,API_VERSION);
	}

	virtual int OnUserPreMessage(User *user, void *dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return 0;

		if (target_type == TYPE_CHANNEL)
		{
			if (((Channel *)dest)->IsExtBanned(user, 'q'))
			{
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Cannot send to " + ((Channel *)dest)->name + ", as you are muted");
				return 1;
			}
		}
		
		return 0;
	}

	virtual int OnUserPreNotice(User *user, void *dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return 0;

		if (target_type == TYPE_CHANNEL)
		{
			if (((Channel *)dest)->IsExtBanned(user, 'q'))
			{
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Cannot send to " + ((Channel *)dest)->name + ", as you are muted");
				return 1;
			}
		}

		return 0;
	}

	virtual void On005Numeric(std::string &output)
	{
		if (output.find(" EXTBAN=:") == std::string::npos)
			output.append(" EXTBAN=:q");
		else
			output.insert(output.find(" EXTBAN=:") + 8, "q");
	}
};


MODULE_INIT(ModuleQuietBan)

