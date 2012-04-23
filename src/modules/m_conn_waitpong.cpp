/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Forces connecting clients to send a PONG message back to the server before they can complete their connection */

class ModuleWaitPong : public Module
{
	bool sendsnotice;
	bool killonbadreply;
	const char* extenstr;

 public:
	ModuleWaitPong(InspIRCd* Me)
	 : Module(Me), extenstr("waitpong_pingstr")
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnUserRegister, I_OnCheckReady, I_OnPreCommand, I_OnRehash, I_OnUserDisconnect, I_OnCleanup };
		ServerInstance->Modules->Attach(eventlist, this, 6);
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf(ServerInstance);

		sendsnotice = Conf.ReadFlag("waitpong", "sendsnotice", 0);

		if(Conf.GetError() == CONF_VALUE_NOT_FOUND)
			sendsnotice = true;

		killonbadreply = Conf.ReadFlag("waitpong", "killonbadreply", 0);

		if(Conf.GetError() == CONF_VALUE_NOT_FOUND)
			killonbadreply = true;
	}


	char* RandString(unsigned int length)
	{
		unsigned char* out = new unsigned char[length+1];
		for(unsigned int i = 0; i < length; i++)
			out[i] = ((rand() % 26) + 65);
		out[length] = '\0';

		return (char*)out;
	}

	virtual int OnUserRegister(User* user)
	{
		char* pingrpl = RandString(10);

		user->Write("PING :%s", pingrpl);

		if(sendsnotice)
			user->WriteServ("NOTICE %s :*** If you are having problems connecting due to ping timeouts, please type /quote PONG %s or /raw PONG %s now.", user->nick.c_str(), pingrpl, pingrpl);

		user->Extend(extenstr, pingrpl);
		return 0;
	}

	virtual int OnPreCommand(std::string &command, std::vector<std::string> &parameters, User* user, bool validated, const std::string &original_line)
	{
		if (command == "PONG")
		{
			char* pingrpl;
			user->GetExt(extenstr, pingrpl);

			if (pingrpl)
			{
				if (!parameters.empty() && (strcmp(pingrpl, parameters[0].c_str()) == 0))
				{
					delete[] pingrpl;
					user->Shrink(extenstr);
					return 1;
				}
				else
				{
					if(killonbadreply)
						ServerInstance->Users->QuitUser(user, "Incorrect ping reply for registration");
					return 1;
				}
			}
		}
		return 0;
	}

	virtual bool OnCheckReady(User* user)
	{
		char* pingrpl;
		return (!user->GetExt(extenstr, pingrpl));
	}

	virtual void OnUserDisconnect(User* user)
	{
		char* pingrpl;
		user->GetExt(extenstr, pingrpl);

		if (pingrpl)
		{
			delete[] pingrpl;
			user->Shrink(extenstr);
		}
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
		{
			User* user = (User*)item;
			char* pingrpl;
			user->GetExt(extenstr, pingrpl);

			if (pingrpl)
			{
				delete[] pingrpl;
				user->Shrink(extenstr);
			}
		}
	}

	virtual ~ModuleWaitPong()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleWaitPong)
