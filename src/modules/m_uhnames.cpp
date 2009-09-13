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
#include "m_cap.h"

/* $ModDesc: Provides the UHNAMES facility. */

class ModuleUHNames : public Module
{
 public:
	GenericCap cap;

	ModuleUHNames(InspIRCd* Me) : Module(Me), cap(this, "userhost-in-names")
	{
		Implementation eventlist[] = { I_OnEvent, I_OnPreCommand, I_OnNamesListItem, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}

	~ModuleUHNames()
	{
	}

	Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}

	void On005Numeric(std::string &output)
	{
		output.append(" UHNAMES");
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
	{
		irc::string c = command.c_str();
		/* We don't actually create a proper command handler class for PROTOCTL,
		 * because other modules might want to have PROTOCTL hooks too.
		 * Therefore, we just hook its as an unvalidated command therefore we
		 * can capture it even if it doesnt exist! :-)
		 */
		if (c == "PROTOCTL")
		{
			if ((parameters.size()) && (!strcasecmp(parameters[0].c_str(),"UHNAMES")))
			{
				cap.ext.set(user, 1);
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void OnNamesListItem(User* issuer, Membership* memb, std::string &prefixes, std::string &nick)
	{
		if (!cap.ext.get(issuer))
			return;

		if (nick.empty())
			return;

		nick = memb->user->GetFullHost();
	}

	void OnEvent(Event* ev)
	{
		cap.HandleEvent(ev);
	}
};

MODULE_INIT(ModuleUHNames)
