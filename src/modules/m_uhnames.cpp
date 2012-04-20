/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "m_cap.h"

/* $ModDesc: Provides the UHNAMES facility. */

class ModuleUHNames : public Module
{
	CUList nl;
 public:

	ModuleUHNames(InspIRCd* Me)
		: Module(Me)
	{
		Implementation eventlist[] = { I_OnEvent, I_OnSyncUserMetaData, I_OnPreCommand, I_OnNamesListItem, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}

	virtual ~ModuleUHNames()
	{
	}

	void OnSyncUserMetaData(User* user, Module* proto,void* opaque, const std::string &extname, bool displayable)
	{
		if ((displayable) && (extname == "UHNAMES"))
			proto->ProtoSendMetaData(opaque, TYPE_USER, user, extname, "Enabled");
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" UHNAMES");
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
			if ((parameters.size()) && (!strcasecmp(parameters[0].c_str(),"UHNAMES")))
			{
				user->Extend("UHNAMES");
				return 1;
			}
		}
		return 0;
	}

	virtual void OnNamesListItem(User* issuer, User* user, Channel* channel, std::string &prefixes, std::string &nick)
	{
		if (!issuer->GetExt("UHNAMES"))
			return;

		if (nick.empty())
			return;

		nick = user->GetFullHost();
	}

	virtual void OnEvent(Event* ev)
	{
		GenericCapHandler(ev, "UHNAMES", "userhost-in-names");
	}
};

MODULE_INIT(ModuleUHNames)
