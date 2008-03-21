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
#include "m_cap.h"

static const char* dummy = "ON";

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
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" NAMESX");
	}

	virtual int OnPreCommand(const std::string &command, const char* const* parameters, int pcnt, User *user, bool validated, const std::string &original_line)
	{
		irc::string c = command.c_str();
		/* We don't actually create a proper command handler class for PROTOCTL,
		 * because other modules might want to have PROTOCTL hooks too.
		 * Therefore, we just hook its as an unvalidated command therefore we
		 * can capture it even if it doesnt exist! :-)
		 */
		if (c == "PROTOCTL")
		{
			if ((pcnt) && (!strcasecmp(parameters[0],"NAMESX")))
			{
				user->Extend("NAMESX",dummy);
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
		if (ev->GetEventID() == "cap_req")
		{
			CapData *data = (CapData *) ev->GetData();

			std::vector<std::string>::iterator it;
			if ((it = std::find(data->wanted.begin(), data->wanted.end(), "multi-prefix")) != data->wanted.end())
			{
				// we can handle this, so ACK it, and remove it from the wanted list
				data->ack.push_back(*it);
				data->wanted.erase(it);
				data->user->Extend("NAMESX",dummy);
			}
		}

		if (ev->GetEventID() == "cap_ls")
		{
			CapData *data = (CapData *) ev->GetData();
			data->wanted.push_back("multi-prefix");
		}

		if (ev->GetEventID() == "cap_list")
		{
			CapData *data = (CapData *) ev->GetData();

			if (data->user->GetExt("NAMESX"))
				data->wanted.push_back("multi-prefix");
		}

		if (ev->GetEventID() == "cap_clear")
		{
			CapData *data = (CapData *) ev->GetData();
			data->ack.push_back("-multi-prefix");
			data->user->Shrink("NAMESX");
		}
	}
};

MODULE_INIT(ModuleNamesX)
