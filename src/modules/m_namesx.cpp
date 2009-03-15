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

/* $ModDesc: Provides the NAMESX (CAP multi-prefix) capability. */

class ModuleNamesX : public Module
{
 public:

	ModuleNamesX(InspIRCd* Me)
		: Module(Me)
	{
		Implementation eventlist[] = { I_OnSyncUserMetaData, I_OnPreCommand, I_OnNamesListItem, I_On005Numeric, I_OnEvent };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}


	virtual ~ModuleNamesX()
	{
	}

	void OnSyncUserMetaData(User* user, Module* proto,void* opaque, const std::string &extname, bool displayable)
	{
		if ((displayable) && (extname == "NAMESX"))
			proto->ProtoSendMetaData(opaque, TYPE_USER, user, extname, "Enabled");
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" NAMESX");
	}

	virtual int OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
	{
		irc::string c = command.c_str();
		/* We don't actually create a proper command handler class for PROTOCTL,
		 * because other modules might want to have PROTOCTL hooks too.
		 * Therefore, we just hook its as an unvalidated command therefore we
		 * can capture it even if it doesnt exist! :-)
		 */
		if (c == "PROTOCTL")
		{
			if ((parameters.size()) && (!strcasecmp(parameters[0].c_str(),"NAMESX")))
			{
				user->Extend("NAMESX");
				return 1;
			}
		}
		return 0;
	}

	virtual void OnNamesListItem(User* issuer, User* user, Channel* channel, std::string &prefixes, std::string &nick)
	{
		if (!issuer->GetExt("NAMESX"))
			return;

		/* Some module hid this from being displayed, dont bother */
		if (nick.empty())
			return;

		prefixes = channel->GetAllPrefixChars(user);
	}

	virtual void OnEvent(Event *ev)
	{
		GenericCapHandler(ev, "NAMESX", "multi-prefix");
	}
};

MODULE_INIT(ModuleNamesX)
