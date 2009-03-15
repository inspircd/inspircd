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

/* $ModDesc: Implements extban +b m: - mute bans */

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
		return Version("$Id$",VF_COMMON|VF_VENDOR,API_VERSION);
	}

	virtual int OnUserPreMessage(User *user, void *dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return 0;

		if (target_type == TYPE_CHANNEL)
		{
			if (((Channel *)dest)->GetExtBanStatus(user, 'm') < 0)
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
			if (((Channel *)dest)->GetExtBanStatus(user, 'm') < 0)
			{
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Cannot send to " + ((Channel *)dest)->name + ", as you are muted");
				return 1;
			}
		}

		return 0;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('m');
	}
};


MODULE_INIT(ModuleQuietBan)

